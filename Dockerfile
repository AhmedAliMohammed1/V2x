FROM ros:jazzy

ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=compute,utility
ENV NVIDIA_REQUIRE_CUDA="cuda>=12.6"
ENV CUDA_MODULE_LOADING=LAZY
ENV MALLOC_ARENA_MAX=2

# Configurable paths
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
    libopencv-dev \
    python3-venv \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install the CUDA runtime libraries required by ONNX Runtime and cuDNN
RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb \
    && dpkg -i cuda-keyring_1.1-1_all.deb \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        cuda-cudart-12-6 \
        cuda-nvrtc-12-6 \
        libcublas-12-6 \
        libcufft-12-6 \
        libcurand-12-6 \
        libcudnn9-cuda-12 \
    && rm -rf /var/lib/apt/lists/* \
    && rm cuda-keyring_1.1-1_all.deb

ENV PATH="/usr/local/cuda/bin:${PATH}"
ENV LD_LIBRARY_PATH="/usr/local/cuda/lib64:/usr/lib/x86_64-linux-gnu"

# rosdep
RUN test -f /etc/ros/rosdep/sources.list.d/20-default.list || rosdep init
RUN rosdep update

# Workspace
WORKDIR ${ROS_WS}

# Install ONNX Runtime GPU in workspace
ARG ONNXRUNTIME_VERSION=1.20.1
RUN wget https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-gpu-${ONNXRUNTIME_VERSION}.tgz -O /tmp/onnx.tgz && \
    mkdir -p ${ONNXRUNTIME_DIR} && \
    tar -xzf /tmp/onnx.tgz -C ${ONNXRUNTIME_DIR} --strip-components=1 && \
    rm /tmp/onnx.tgz && \
    echo "${ONNXRUNTIME_DIR}/lib" > /etc/ld.so.conf.d/onnxruntime.conf && \
    ldconfig

ENV LD_LIBRARY_PATH="${ONNXRUNTIME_DIR}/lib:${LD_LIBRARY_PATH}"

# Build the source from this Docker build context.
RUN mkdir -p ${ROS_WS}/src
COPY . ${ROS_WS}/src/V2x

# Install dependencies from package.xml
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && \
    apt-get update && \
    cd ${ROS_WS} && \
    rosdep install --rosdistro jazzy --from-paths src --ignore-src -r -y && \
    rm -rf /var/lib/apt/lists/*"

# Verify ONNX Runtime before build
RUN /bin/bash -c "echo ONNXRUNTIME_DIR=${ONNXRUNTIME_DIR} && \
    find ${ONNXRUNTIME_DIR} -name onnxruntime_cxx_api.h && \
    find ${ONNXRUNTIME_DIR} -name 'libonnxruntime.so*' && \
    test -f ${ONNXRUNTIME_DIR}/lib/libonnxruntime_providers_cuda.so && \
    ! ldd ${ONNXRUNTIME_DIR}/lib/libonnxruntime_providers_cuda.so | grep -q 'not found'"

# Build ROS 2 workspace
ARG ENABLE_GPU=ON
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && \
    cd ${ROS_WS} && \
    colcon build --packages-select ros2_yolos_cpp --cmake-args \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DENABLE_GPU=${ENABLE_GPU} \
    -DONNXRUNTIME_DIR=${ONNXRUNTIME_DIR} && \
    colcon build --packages-select late_fusion_for_yolos_cpp --cmake-args \
    -DCMAKE_BUILD_TYPE=Release"

# Make environment available for any user
RUN echo "export ROS_WS=${ROS_WS}" >> /etc/bash.bashrc && \
    echo "export ONNXRUNTIME_DIR=${ONNXRUNTIME_DIR}" >> /etc/bash.bashrc && \
    echo "export ONNXRUNTIME_ROOT=${ONNXRUNTIME_ROOT}" >> /etc/bash.bashrc && \
    echo "export LD_LIBRARY_PATH=${ONNXRUNTIME_DIR}/lib:\$LD_LIBRARY_PATH" >> /etc/bash.bashrc && \
    echo "source /opt/ros/jazzy/setup.bash" >> /etc/bash.bashrc && \
    echo "if [ -f ${ROS_WS}/install/setup.bash ]; then source ${ROS_WS}/install/setup.bash; fi" >> /etc/bash.bashrc

WORKDIR ${ROS_WS}

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD nvidia-smi >/dev/null 2>&1 || exit 1

CMD ["sleep", "infinity"]
