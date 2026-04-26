FROM ros:jazzy

# Configure NVIDIA driver capabilities for GPU passthrough
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics,display

# Install essential build tools and common dependencies
RUN apt-get update && apt-get install -y \
    python3-colcon-common-extensions \
    python3-rosdep \
    build-essential \
    cmake \
    wget \
    curl \
    gnupg2 \
    tar \
    git \
    tmux \
    ros-jazzy-rviz2 \
    libopencv-dev \
    python3-venv \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install CUDA Toolkit and cuDNN (Required by ONNX Runtime GPU)
# Note: NVIDIA Container Toolkit runs on the host machine, not inside the container.
# Inside the container, we need the CUDA runtime libraries for GPU acceleration.
RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb \
    && dpkg -i cuda-keyring_1.1-1_all.deb \
    && apt-get update \
    && apt-get install -y cuda-toolkit-12-6 libcudnn9-cuda-12 \
    && rm -rf /var/lib/apt/lists/* \
    && rm cuda-keyring_1.1-1_all.deb

# Add CUDA to library and binary paths
ENV LD_LIBRARY_PATH="/usr/local/cuda/lib64"
ENV PATH="/usr/local/cuda/bin:${PATH}"

# Run rosdep init and update
RUN rosdep init || true
RUN rosdep update

# Download and install ONNX Runtime for GPU (required for ros2_yolos_cpp GPU acceleration)
RUN wget https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-linux-x64-gpu-1.20.1.tgz -O /tmp/onnx.tgz && \
    mkdir -p /opt/onnxruntime && \
    tar -xzf /tmp/onnx.tgz -C /opt/onnxruntime --strip-components=1 && \
    rm /tmp/onnx.tgz && \
    echo "/opt/onnxruntime/lib" > /etc/ld.so.conf.d/onnxruntime.conf && \
    ldconfig

ENV ONNXRUNTIME_DIR=/opt/onnxruntime
ENV LD_LIBRARY_PATH="/opt/onnxruntime/lib:${LD_LIBRARY_PATH}"

# Set up the workspace
WORKDIR /ros2_ws

# Clone the workspace source code from GitHub
RUN mkdir -p src && \
    git clone https://github.com/AhmedAliMohammed1/V2x.git

RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && \
    apt-get update && \
    cd /ros2_ws && \
    rosdep install --rosdistro jazzy --from-paths src --ignore-src -r -y && \
    rm -rf /var/lib/apt/lists/*"
RUN apt-get update && apt-get install -y \
    ros-jazzy-cv-bridge \
    ros-jazzy-image-transport \
    ros-jazzy-vision-msgs \
    ros-jazzy-rclcpp-components \
    ros-jazzy-rclcpp-lifecycle \
    ros-jazzy-lifecycle-msgs \
    && rm -rf /var/lib/apt/lists/*
# Build the ROS 2 workspace
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && \
    cd /ros2_ws && \
    colcon build --cmake-args \
    -DCMAKE_BUILD_TYPE=Release \
    -DONNXRUNTIME_DIR=/opt/onnxruntime"
# Entrypoint setup to source ROS 2 and workspace on container start
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
RUN echo "if [ -f /ros2_ws/install/setup.bash ]; then source /ros2_ws/install/setup.bash; fi" >> ~/.bashrc

CMD ["bash"]
