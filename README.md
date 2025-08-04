# Programowanie-rozproszone

Projekt zaliczeniowy z przedmiotu "Programowanie rozproszone"

Polecenie:
Stowarzyszenie babci i wnusi

Grupa emerytek bardzo chciałaby robić konfitury dla wnuczek, tylko że niestety wnuczek nie mają. Na szczęście grupa studentek Uniwersytetu Artystycznego chciałaby, by ktoś je dokarmiał - więc wspólnie emerytki i studentki założyły stowarzyszenie, gdzie babcie tworzą konfitury, a studentki je zjadają, po czym oddają słoiki emerytkom.

Procesy: B babć i S studentek
Zasoby: P słoików, K konfitur - ale P i K to wartości maksymalne
Wymogi: Babcie ubiegają się o słoiki. Gdy mają słoik, tworzą konfiturę. Studentki ubiegają się o konfitury. Gdy dostaną konfiturę, zjadają ją i oddają słoik.

Procesy działają z różną prędkością, mogą wręcz przez jakiś czas nie chcieć robić albo jeść konfitur. Nie powinno to blokować pracy innych procesów.
