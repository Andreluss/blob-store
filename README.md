# blob-store
Design doc: https://docs.google.com/document/d/1bGVrjKyt-A_iQEfeR2zDWv8M_Ba6Xc5BvzLR6KGmDzg/edit?usp=sharing

## Docker
Instalacja:
https://docs.docker.com/engine/install/ubuntu/
Żeby nie wpisywać sudo za każdym razem:
https://www.digitalocean.com/community/questions/how-to-fix-docker-got-permission-denied-while-trying-to-connect-to-the-docker-daemon-socket
oraz to:
`sudo setfacl --modify user:<user name or ID>:rw /var/run/docker.sock`

Gdy mamy już docker to należy zbudować obraz deweloperski, jest skrypt dev.sh
`./dev.sh buid-image`
Może to zająć nawet >10 minut za pierwszym razem (pobiera i buduje gRPC from source w konterze)

## Konfiguracja IDE
W Clionie należy wejść w settings->Build,Execution,Deployment->Toolchains
i stwrozyć nowy toolchain Docker. Wybrać obraz blob-store, Container settings -> volumes i tutaj dodać dwie wartości:
host: <nasz scieżka do projektu>/blob-store
container: /usr/src/app
host: <nasza scieżka do projektu>/blob-store/build
container: /usr/src/app/build

Reszta ustawień defaultowa.

Następnie wejść w settings->Build,Execution,Deployment->CMake i stworzyć nowy profil. 
Wybrać dla niego Build type: default, toolchain: nasz docker

Żeby sprawdzić czy wszystko działa to 
1. Reload CMake Project w IDE
2. Wejść w src/master/main.cpp
3. Zbuildować go za pomocą IDE (lub uruchomić)
4. Po tych krokach IDE nie powinno podświetlać niczego na czerwono w kodzie, a program powinien się uruchamiać.