# Multi-stage build for Cross-Platform Media Player
FROM ubuntu:22.04 as builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    pkg-config \
    libvlc-dev \
    libvlccore-dev \
    vlc \
    libva-dev \
    libvdpau-dev \
    libx11-dev \
    libasound2-dev \
    libpulse-dev \
    qt6-base-dev \
    qt6-multimedia-dev \
    libqt6multimedia6 \
    libqt6multimediawidgets6 \
    qt6-base-dev-tools \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Configure and build
RUN cmake -B build -S . \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=ON

RUN cmake --build build --parallel

# Run tests
RUN cd build && ctest --output-on-failure --parallel

# Install the application
RUN cmake --install build

# Runtime stage
FROM ubuntu:22.04 as runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libvlc5 \
    vlc-plugin-base \
    vlc-plugin-video-output \
    vlc-plugin-audio-output \
    libva2 \
    libvdpau1 \
    libx11-6 \
    libasound2 \
    libpulse0 \
    libqt6core6 \
    libqt6gui6 \
    libqt6widgets6 \
    libqt6multimedia6 \
    libqt6multimediawidgets6 \
    libqt6network6 \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# Copy built application from builder stage
COPY --from=builder /usr/local /usr/local

# Create non-root user
RUN useradd -m -s /bin/bash mediaplayer

# Set up environment
ENV QT_QPA_PLATFORM=xcb
ENV DISPLAY=:99

# Create startup script
RUN echo '#!/bin/bash\n\
# Start virtual display for headless operation\n\
Xvfb :99 -screen 0 1024x768x24 &\n\
export DISPLAY=:99\n\
\n\
# Start EonPlay\n\
exec /usr/local/bin/EonPlay "$@"' > /usr/local/bin/start-eonplay.sh

RUN chmod +x /usr/local/bin/start-eonplay.sh

# Switch to non-root user
USER mediaplayer
WORKDIR /home/mediaplayer

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD pgrep -f EonPlay || exit 1

# Expose any ports if needed (for remote control, streaming, etc.)
# EXPOSE 8080

# Default command
CMD ["/usr/local/bin/start-eonplay.sh"]

# Labels for metadata
LABEL org.opencontainers.image.title="EonPlay"
LABEL org.opencontainers.image.description="Timeless, futuristic media player - play for eons"
LABEL org.opencontainers.image.version="1.0.0"
LABEL org.opencontainers.image.vendor="EonPlay Team"
LABEL org.opencontainers.image.licenses="GPL-3.0"