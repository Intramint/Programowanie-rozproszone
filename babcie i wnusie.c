#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define MSG_REQ 1
#define MSG_ACK 2
#define MSG_UPDATE_RESOURCE 3

#define MAX_QUEUE 100
#define MAX_PROCESSES 100

#define ROLE_GRANDMA 0
#define ROLE_STUDENT 1


typedef struct {
    int timestamp;
    int process_id;
} Request;

int myId, size;
int role; // ROLE_GRANDMA or ROLE_STUDENT

int lamport_clock = 0;
int last_timestamps[MAX_PROCESSES];
Request requests_queue[MAX_QUEUE];
int requests_count = 0;


int ack_count = 0;
pthread_mutex_t ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ack_all = PTHREAD_COND_INITIALIZER;

pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t resource_change = PTHREAD_COND_INITIALIZER;

int P = 0; // liczba słoików
int K = 0; // liczba konfitur
int B = 0, S = 0; // liczba babć i studentek

char whichRole(int role){
    if (role == 1)
        return 'S';
    else
        return 'B';
}

void increment_lamport(int received_ts) {
    lamport_clock = (lamport_clock > received_ts ? lamport_clock : received_ts) + 1;
}

void add_request(Request r) {
    requests_queue[requests_count++] = r;
    for (int i = 0; i < requests_count - 1; i++) {
        for (int j = 0; j < requests_count - i - 1; j++) {
            if (requests_queue[j].timestamp > requests_queue[j + 1].timestamp ||
                (requests_queue[j].timestamp == requests_queue[j + 1].timestamp &&
                 requests_queue[j].process_id > requests_queue[j + 1].process_id)) {
                Request temp = requests_queue[j];
                requests_queue[j] = requests_queue[j + 1];
                requests_queue[j + 1] = temp;
            }
        }
    }
}

bool can_enter_critical_section() {
    return (requests_queue[0].process_id == myId) &&
           ((role == ROLE_GRANDMA && P > 0) ||
            (role == ROLE_STUDENT && K > 0));
}

void* message_handler(void* arg) {
    MPI_Status status;
    int message[2];
    while (1) {
        MPI_Recv(message, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int sender = status.MPI_SOURCE;
        int msg_type = status.MPI_TAG;
        int timestamp = message[0];
        increment_lamport(timestamp);

        if (timestamp >= last_timestamps[sender]) {
            last_timestamps[sender] = timestamp;

            if (msg_type == MSG_REQ) {
                printf("[%d] [%c] [t%d] Odebrałem REQ od [%d] - {ts:[%d], id:[%d]}\n", myId,whichRole(role), lamport_clock, sender, timestamp, sender);
                Request r = {timestamp, sender};
                add_request(r);
                lamport_clock++;
                int ack_msg[1] = {lamport_clock};
                MPI_Send(ack_msg, 1, MPI_INT, sender, MSG_ACK, MPI_COMM_WORLD);
            } else if (msg_type == MSG_ACK) {
                printf("[%d] [%c] [t%d] Odebrałem ACK od [%d]\n", myId,whichRole(role), lamport_clock, sender);
                pthread_mutex_lock(&ack_mutex);
                ack_count++;
                if ((role == ROLE_GRANDMA && ack_count >= B - 1) ||
                    (role == ROLE_STUDENT && ack_count >= S - 1)) {
                    pthread_cond_signal(&ack_all);
                }
                pthread_mutex_unlock(&ack_mutex);
            } else if (msg_type == MSG_UPDATE_RESOURCE) {
                printf("[%d] [%c] [t%d] Odebrałem UPDATE_RESOURCE od [%d]\n", myId,whichRole(role), lamport_clock, sender);
                int msg_delta = message[1];
                pthread_mutex_lock(&resource_mutex);
                if (role == ROLE_GRANDMA) {
                    P += msg_delta;
                    printf("[%d] [%c] [t%d] Jest teraz tyle słoików: [%d]\n", myId,whichRole(role), lamport_clock, P);
                } else {
                    K += msg_delta;
                    printf("[%d] [%c] [t%d] Jest teraz tyle konfitur: [%d]\n", myId,whichRole(role), lamport_clock, K);
                }
                if (msg_delta < 0) {
                    requests_count--;
                    for (int i = 0; i < requests_count; i++) {
                        requests_queue[i] = requests_queue[i + 1];
                    }
                }
                pthread_cond_signal(&resource_change);
                pthread_mutex_unlock(&resource_mutex);
            }
        }
    }
    return NULL;
}

void send_request_to_group(int group_size, int group_role) {
   int message[2] = {lamport_clock, 0};
    for (int i = 0; i < size; i++) {
        if (i != myId && ((group_role == ROLE_GRANDMA && i < B) ||
                          (group_role == ROLE_STUDENT && i >= B))) {
            MPI_Send(message, 2, MPI_INT, i, MSG_REQ, MPI_COMM_WORLD);
        }
    }
}

void send_update_resource(int delta, int group_role) {
    int message[2] = {lamport_clock, delta};
    for (int i = 0; i < size; i++) {
        if ((group_role == ROLE_GRANDMA && i < B) ||
            (group_role == ROLE_STUDENT && i >= B)) {
            MPI_Send(message, 2, MPI_INT, i, MSG_UPDATE_RESOURCE, MPI_COMM_WORLD);
        }
    }
}

void grandma_loop() {
    while (1) {
        sleep(rand() % 8 + 1);
        lamport_clock++;
        printf("[%d] [B] [t%d] Rozpoczynam staranie o sekcję krytyczną\n", myId, lamport_clock);

        Request my_request = {lamport_clock, myId};
        add_request(my_request);
        // printf("[%d] [B] [t%d] requests_count: [%d]\n", myId, lamport_clock, requests_count);
        // for (int i = 0; i < requests_count; i++) {
        //     printf("[%d] [B] [t%d] ts: [%d], id: [%d]\n", myId, lamport_clock, requests_queue[i].timestamp, requests_queue[i].process_id);
        // }
        

        ack_count = 0;
        send_request_to_group(B, ROLE_GRANDMA);

        pthread_mutex_lock(&ack_mutex);
        while (ack_count < B - 1) {
            pthread_cond_wait(&ack_all, &ack_mutex);
        }
        pthread_mutex_unlock(&ack_mutex);

        pthread_mutex_lock(&resource_mutex);
        while (!can_enter_critical_section()) {
            // printf("[%d] [B] [t%d] cond check\n", myId, lamport_clock);
            // printf("[%d] [B] [t%d] requests_count: [%d]\n", myId, lamport_clock, requests_count);
            // for (int i = 0; i < 10; i++) {
            //     printf("[%d] [B] [t%d] %d - ts: [%d], id: [%d]\n", myId, lamport_clock, i, requests_queue[i].timestamp, requests_queue[i].process_id);
            // }
            pthread_cond_wait(&resource_change, &resource_mutex);
        }

        lamport_clock++;
        printf("[%d] [B] [t%d] Jestem w sekcji krytycznej\n", myId, lamport_clock);

        send_update_resource(-1, ROLE_GRANDMA);
        send_update_resource(1, ROLE_STUDENT);

        // requests_count--;
        // for (int i = 0; i < requests_count; i++) {
        //     requests_queue[i] = requests_queue[i + 1];
        // }

        lamport_clock++;
        printf("[%d] [B] [t%d] Wychodzę z sekcji krytycznej\n", myId, lamport_clock);
        pthread_mutex_unlock(&resource_mutex);
    }
}

void student_loop() {
    while (1) {
        sleep(rand() % 8 + 1);
        lamport_clock++;
        printf("[%d] [S] [t%d] Rozpoczynam staranie o sekcję krytyczną\n", myId, lamport_clock);

        Request my_request = {lamport_clock, myId};
        add_request(my_request);

        ack_count = 0;
        send_request_to_group(S, ROLE_STUDENT);

        pthread_mutex_lock(&ack_mutex);
        while (ack_count < S - 1) {
            pthread_cond_wait(&ack_all, &ack_mutex);
        }
        pthread_mutex_unlock(&ack_mutex);

        pthread_mutex_lock(&resource_mutex);
        while (!can_enter_critical_section()) {
            pthread_cond_wait(&resource_change, &resource_mutex);
        }

        lamport_clock++;
        printf("[%d] [S] [t%d] Jestem w sekcji krytycznej\n", myId, lamport_clock);

        send_update_resource(-1, ROLE_STUDENT);
        send_update_resource(1, ROLE_GRANDMA);

        // requests_count--;
        // for (int i = 0; i < requests_count; i++) {
        //     requests_queue[i] = requests_queue[i + 1];
        // }

        lamport_clock++;
        printf("[%d] [S] [t%d] Wychodzę z sekcji krytycznej\n", myId, lamport_clock);
        pthread_mutex_unlock(&resource_mutex);
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myId);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 5) {
        if (myId == 0)
            printf("Usage: mpirun -np <B+S> ./program <B> <S> <P> <K>\n");
        MPI_Finalize();
        return 1;
    }

    B = atoi(argv[1]);
    S = atoi(argv[2]);
    P = atoi(argv[3]);
    K = atoi(argv[4]);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        last_timestamps[i] = 0;
    }

    role = (myId < B) ? ROLE_GRANDMA : ROLE_STUDENT;

    pthread_t listener;
    pthread_create(&listener, NULL, message_handler, NULL);

    if (role == ROLE_GRANDMA)
        grandma_loop();
    else
        student_loop();

    pthread_join(listener, NULL);
    MPI_Finalize();
    return 0;
}
