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
Może to zająć nawet >10 minut za pierwszym razem (instaluje wszystkie dependencies)

## Konfiguracja IDE
W Clionie należy wejść w settings->Build,Execution,Deployment->Toolchains
i stwrozyć nowy toolchain Docker. Wybrać obraz blob-store, Container settings -> volumes i tutaj dodać dwie wartości:

host: \<nasz scieżka do projektu\>/blob-store
container: /usr/src/app

host: \<nasza scieżka do projektu\>/blob-store/build
container: /usr/src/app/build

Reszta ustawień defaultowa.

Następnie wejść w settings->Build,Execution,Deployment->CMake zmodyfikować profil. 
Wybrać dla niego toolchain: nasz docker i dodać opcje CMake:
`-B build -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake`

Żeby sprawdzić czy wszystko działa to 
1. Reload CMake Project w IDE
2. Wejść w src/master/main.cpp
3. Zbuildować go za pomocą IDE (lub uruchomić)
4. Po tych krokach IDE nie powinno podświetlać niczego na czerwono w kodzie, a program powinien się uruchamiać.

## Kubernetes
### Nasz config:
- klaster z image registry: `europe-central2-docker.pkg.dev`
- nasz blob image registry: `europe-central2-docker.pkg.dev/blobs69/blob-repository`
- nazwa dockerimage: `blob-store` (tag: `latest`)
### Na początku
`gcloud auth configure-docker europe-central2-docker.pkg.dev`
### Po wprowadzeniu zmian
1. Po wprowadzeniu zmian `./dev.sh build-image`
   - wynik można podejrzeć w `docker images`
2. `docker tag blob-store europe-central2-docker.pkg.dev/blobs69/blob-repository/blob-store:latest`
3. `docker push europe-central2-docker.pkg.dev/blobs69/blob-repository/blob-store:latest`