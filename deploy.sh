#!/bin/bash

docker container rm -f klotski-container
docker image rm -f klotski-image
docker build -t klotski-image .
docker run -d -p 8080:8080 --name klotski-container klotski-image
