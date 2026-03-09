# Create build environments

docker build Android/ --platform linux/amd64 -t cobble-android-build
docker build Linux/ --platform linux/amd64 -t cobble-linux-build
