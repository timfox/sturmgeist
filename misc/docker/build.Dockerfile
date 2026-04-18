FROM rockylinux/9
LABEL version="2.0"
LABEL maintainer="mail@etlegacy.com"
LABEL description="Linux build machine for the 32 and 64 bit linux releases (Rocky Linux 9, multilib)"

# Multilib allows 32-bit (-m32) builds alongside x86_64.
RUN echo "multilib_policy=all" >> /etc/dnf/dnf.conf

# Base OS + CRB (build deps) + EPEL; then toolchain and graphics/audio stack.
RUN dnf -y update && \
	dnf -y install epel-release dnf-plugins-core && \
	dnf config-manager --set-enabled crb && \
	dnf -y groupinstall "Development Tools" && \
	dnf -y install --allowerasing \
	git \
	tar make gcc gcc-c++ \
	glibc-devel glibc-devel.i686 libstdc++-devel.i686 \
	libgcc.i686 \
	freeglut-devel \
	libX11 libX11-devel \
	mesa-libGL mesa-libGL-devel \
	alsa-lib-devel \
	pulseaudio-libs-devel \
	libcurl-devel zlib-devel wget nasm which \
	libXxf86vm-devel perl-IPC-Cmd \
	wayland-devel mesa-libEGL-devel mesa-libGLES-devel \
	libxkbcommon-devel libXi-devel libXfixes-devel \
	libXScrnSaver-devel libXcursor-devel libXinerama-devel libXrandr-devel libXvmc-devel \
	perl-Thread-Queue \
	libffi-devel expat-devel libxml2-devel \
	pkgconf-pkg-config \
	&& dnf clean all && \
	rm -rf /var/cache/dnf /var/tmp/dnf-*

RUN wget https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.gz && tar -xvzf m4-1.4.19.tar.gz && cd m4-1.4.19 && ./configure --prefix=/usr/local && make && make install && cd .. && \
	wget https://ftp.gnu.org/pub/gnu/libtool/libtool-2.4.7.tar.gz && tar -xvzf libtool-2.4.7.tar.gz && cd libtool-2.4.7 && ./configure --prefix=/usr/local && make && make install && cd .. && \
	wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.71.tar.gz && tar -xvzf autoconf-2.71.tar.gz && cd autoconf-2.71 && ./configure --prefix=/usr/local && make && make install && cd .. && \
	wget https://ftp.gnu.org/gnu/automake/automake-1.15.tar.gz && tar -xvzf automake-1.15.tar.gz && cd automake-1.15 && ./configure --prefix=/usr/local && make && make install && cd .. && \
	rm -Rf m4-1.4.19* libtool-2.4.7* autoconf-2.71* automake-1.15*

RUN mkdir -p /opt/cmake && wget --no-check-certificate --quiet -O - https://cmake.org/files/v3.28/cmake-3.28.2-linux-x86_64.tar.gz | tar --strip-components=1 -xz -C /opt/cmake
ENV PATH="/opt/cmake/bin:${PATH}"

# SDL2 expects wayland >= 1.18: build 64- and 32-bit wayland from source (same layout as EL7 image).
RUN rpm -e --nodeps --allmatches libwayland-client libwayland-cursor libwayland-egl libwayland-server wayland-devel 2>/dev/null || true && \
	dnf -y install libffi-devel expat-devel libxml2-devel && dnf clean all && rm -rf /var/cache/dnf /var/tmp/dnf-* && \
	wget --quiet -O - https://wayland.freedesktop.org/releases/wayland-1.18.0.tar.xz | tar -xJ && cd wayland-1.18.0 && \
	export PKG_CONFIG_PATH=/usr/lib64/pkgconfig && \
	./configure --prefix=/usr --disable-static --disable-documentation --libdir=/usr/lib64 && make && make install && \
	make clean && export PKG_CONFIG_PATH=/usr/lib/pkgconfig && \
	./configure --prefix=/usr --disable-static --disable-documentation --libdir=/usr/lib --host=i686-linux-gnu "CFLAGS=-m32" "CXXFLAGS=-m32" "LDFLAGS=-m32" && make && make install && \
	cd .. && rm -Rf wayland-1.18.0 && unset PKG_CONFIG_PATH

RUN git clone --branch v1.11.1 --depth 1 https://github.com/ninja-build/ninja.git && cmake -B ninja/build -S ninja && cmake --build ninja/build && \
	cmake --install ninja/build && rm -Rf ninja

VOLUME /code
WORKDIR /code
