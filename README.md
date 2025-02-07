# blob-store
Design doc: https://docs.google.com/document/d/1bGVrjKyt-A_iQEfeR2zDWv8M_Ba6Xc5BvzLR6KGmDzg/edit?usp=sharing

## Docker
Installation:
https://docs.docker.com/engine/install/ubuntu/
To avoid typing sudo every time:
https://www.digitalocean.com/community/questions/how-to-fix-docker-got-permission-denied-while-trying-to-connect-to-the-docker-daemon-socket
and this:
`sudo setfacl --modify user:<user name or ID>:rw /var/run/docker.sock`

Once we have docker, we need to build a development image, there is a script dev.sh
`./dev.sh buid-image`
This may take >10 minutes the first time (installs all dependencies)

## IDE configuration
In Clion, go to settings->Build,Execution,Deployment->Toolchains
and create a new Docker toolchain. Select the blob-store image, Container settings -> volumes and add two values ​​here:

host: \<our project path\>/blob-store
container: /usr/src/app

host: \<our project path\>/blob-store/build
container: /usr/src/app/build

The rest of the settings are default.

Then go to settings->Build,Execution,Deployment->CMake and modify the profile.

Select the toolchain for it: our docker and add CMake options:

`-B build -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake`

To check if everything works,

1. Reload CMake Project in the IDE
2. Go to src/master/main.cpp
3. Build it using the IDE (or run it)
4. After these steps, the IDE should not highlight anything in red in the code, and the program should start.

## Kubernetes
### Our config:
- cluster from blob-store: `blobs-cluster` region: `europe-central2`
- cluster from image registry: `europe-central2-docker.pkg.dev`
- registry image repo: `europe-central2-docker.pkg.dev/blobs69/blob-repository`
- dockerimage name: `blob-store` (tag: `latest` or `v1.0.70`)
- permissions to google-cloud-api's from the pod: https://cloud.google.com/kubernetes-engine/docs/how-to/workload-identity#kubernetes-sa-to-iam
### At the beginning
```
gcloud auth configure-docker europe-central2-docker.pkg.dev
gcloud container clusters get-credentials blobs-cluster --region europe-central2

kubectl get pods # should work
```

### After making changes
1. After making changes build and push to registry (Local - CLion):
```
./dev.sh push-docker
```
2. Reloading everything (GCloud):
```
kubectl delete all --all
```
3. Deployment (GCloud):
```
cd /path/to/k8s/configs # can be downloaded from repo
kubectl apply -f master.yaml
kubectl apply -f frontend.yaml
kubectl apply -f worker.yaml
```

## Success
![image](https://github.com/user-attachments/assets/35993753-22b2-4959-927f-2cd596f95162)
