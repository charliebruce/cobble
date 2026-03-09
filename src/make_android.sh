mkdir -p build

docker run --rm --platform linux/amd64 -v $(pwd):/workspace -w /workspace cobble-android-build /bin/bash /workspace/make_android_within_docker.sh