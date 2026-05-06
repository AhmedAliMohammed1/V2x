FROM ros:jazzy

ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics,display

# Configurable paths, not user-specific
ENV ROS_WS=/ros2_ws
ENV ONNXRUNTIME_DIR=/ros2_ws/onnxruntime
ENV ONNXRUNTIME_ROOT=/ros2_ws/onnxruntime

# Install base dependencies
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

# Install CUDA Toolkit and cuDNN
RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb \
    && dpkg -i cuda-keyring_1.1-1_all.deb \
    && apt-get update \
    && apt-get install -y cuda-toolkit-12-6 libcudnn9-cuda-12 \
    && rm -rf /var/lib/apt/lists/* \
    && rm cuda-keyring_1.1-1_all.deb

ENV PATH="/usr/local/cuda/bin:${PATH}"
ENV LD_LIBRARY_PATH="/usr/local/cuda/lib64"

# rosdep
RUN rosdep init || true
RUN rosdep update

# Workspace
WORKDIR ${ROS_WS}

# Install ONNX Runtime GPU in workspace, not /root or /opt
RUN wget https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-linux-x64-gpu-1.20.1.tgz -O /tmp/onnx.tgz && \
    mkdir -p ${ONNXRUNTIME_DIR} && \
    tar -xzf /tmp/onnx.tgz -C ${ONNXRUNTIME_DIR} --strip-components=1 && \
    rm /tmp/onnx.tgz && \
    echo "${ONNXRUNTIME_DIR}/lib" > /etc/ld.so.conf.d/onnxruntime.conf && \
    ldconfig

ENV LD_LIBRARY_PATH="${ONNXRUNTIME_DIR}/lib:${LD_LIBRARY_PATH}"

# Clone workspace source code
RUN mkdir -p src && \
    git clone https://github.com/AhmedAliMohammed1/V2x.git

# Install dependencies from package.xml
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && \
    apt-get update && \
    cd ${ROS_WS} && \
    rosdep install --rosdistro jazzy --from-paths src --ignore-src -r -y && \
    rm -rf /var/lib/apt/lists/*"

# Extra ROS dependencies
RUN apt-get update && apt-get install -y \
    ros-jazzy-cv-bridge \
    ros-jazzy-image-transport \
    ros-jazzy-vision-msgs \
    ros-jazzy-rclcpp-components \
    ros-jazzy-rclcpp-lifecycle \
    ros-jazzy-lifecycle-msgs \
    && rm -rf /var/lib/apt/lists/*

# Build ROS 2 workspace
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && \
    cd ${ROS_WS} && \
    colcon build --cmake-args \
    -DCMAKE_BUILD_TYPE=Release \
    -DONNXRUNTIME_DIR=${ONNXRUNTIME_DIR}"

# Make environment available for any user
RUN echo "export ONNXRUNTIME_DIR=${ONNXRUNTIME_DIR}" >> /etc/bash.bashrc && \
    echo "export ONNXRUNTIME_ROOT=${ONNXRUNTIME_ROOT}" >> /etc/bash.bashrc && \
    echo "export LD_LIBRARY_PATH=${ONNXRUNTIME_DIR}/lib:\$LD_LIBRARY_PATH" >> /etc/bash.bashrc && \
    echo "source /opt/ros/jazzy/setup.bash" >> /etc/bash.bashrc && \
    echo "if [ -f ${ROS_WS}/install/setup.bash ]; then source ${ROS_WS}/install/setup.bash; fi" >> /etc/bash.bashrc

WORKDIR ${ROS_WS}

CMD ["bash"]
