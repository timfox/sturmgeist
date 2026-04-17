# Pinned digest for thyrlian/android-sdk:latest (amd64) — update via Docker Hub when rebuilding the image intentionally.
FROM thyrlian/android-sdk@sha256:bb9ed3686968550d927228777bca787dd7913e679f1e73e85525ba0094ea170d
LABEL version="1.0"
LABEL maintainer="mail@etlegacy.com"
LABEL description="Linux build machine for the android releases"

# Upgrade the system to be the most up to date
# We will later decide which libs to install
RUN apt update && apt upgrade -y && apt install ninja-build rename patch -y && apt autopurge -y && apt clean

VOLUME /code
WORKDIR /code
