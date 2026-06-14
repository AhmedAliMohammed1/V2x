# Late Fusion YOLOs CPP

<p align="center">
  <img src="late_fusion_for_yolos_cpp/assets/Banner.png" width="100%"/>
</p>

A ROS 2 Jazzy multi-camera perception pipeline that runs YOLO inference for each camera and combines the annotated streams into a panoramic late-fusion view.

## Capabilities

- Runs 1-6 independent camera detectors.
- Supports CPU inference and validated NVIDIA GPU inference.
- Uses one detector per process to isolate CUDA contexts and avoid inference/lifecycle races.
- Keeps only the newest camera frame to prevent queued-frame latency.
- Disables debug-image rendering and timing publication by default for minimum overhead.
- Automatically configures and activates detector lifecycle nodes from the detector launch files.
- Automatically restarts a detector after fatal CUDA context errors such as CUDA error 702.
- Validates ONNX Runtime, its CUDA provider, CUDA runtime libraries, and cuDNN compatibility during GPU builds.
- Provides host-side and in-container tmuxinator startup profiles.
- Publishes ROS 2 detections, optional annotated images, optional timing metrics, and a fused panoramic image.

## Architecture

1. Each camera runs in its own standalone lifecycle-node process.
2. Each detector subscribes with sensor-data QoS and a queue depth of one.
3. The late-fusion node combines fresh detector outputs into the configured panoramic layout.
4. If ONNX Runtime reports a poisoned CUDA context, the affected detector exits and its launch supervisor recreates and reactivates it.

CUDA error 702 means a GPU kernel exceeded the driver's allowed execution time. The CUDA context is unusable afterward, which causes follow-on errors such as `CUBLAS_STATUS_NOT_INITIALIZED`. It is usually caused by GPU overload, a driver watchdog, thermal/power instability, or an incompatible runtime stack, rather than a missing application library. The project now recovers at the process boundary because recovery inside the same CUDA context is unsafe.

## Requirements

- ROS 2 Jazzy
- OpenCV 4.5+
- ONNX Runtime 1.20.1 or another compatible release
- C++17 compiler and CMake 3.16+
- For Docker GPU mode: NVIDIA driver reporting CUDA 12.6 or newer and NVIDIA Container Toolkit; the image supplies CUDA and cuDNN

## Build From Source

Create a ROS workspace and clone this repository under its `src` directory:

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
git clone https://github.com/Pavankumarsp02/late_fusion_yolos_cpp.git V2x
cd ~/ros2_ws
rosdep update
rosdep install --from-paths src --ignore-src -r -y
```

Set `ONNXRUNTIME_DIR` to an extracted ONNX Runtime installation containing `include/` and `lib/`.

### CPU-Compatible Build

CPU-compatible builds are the default and do not require the CUDA provider:

```bash
export ONNXRUNTIME_DIR=/opt/onnxruntime
colcon build --cmake-args \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_GPU=OFF \
  -DONNXRUNTIME_DIR="$ONNXRUNTIME_DIR"
source install/setup.bash
```

Launch with `use_gpu:=false`.

### GPU-Validated Build

GPU builds fail early when the ONNX Runtime CUDA provider or one of its runtime dependencies is missing:

```bash
export ONNXRUNTIME_DIR=/opt/onnxruntime-gpu
colcon build --cmake-args \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_GPU=ON \
  -DONNXRUNTIME_DIR="$ONNXRUNTIME_DIR"
source install/setup.bash
```

Launch with `use_gpu:=true` only after the GPU validation succeeds.

## Docker GPU Setup

Docker is the recommended GPU deployment path because it pins CUDA 12.6, cuDNN 9, and ONNX Runtime GPU 1.20.1 together.

Build the image:

```bash
docker build -t v2x-gpu .
```

Create the persistent container once:

```bash
docker run -d \
  --name v2x-gpu-container \
  --restart unless-stopped \
  --gpus all \
  --network host \
  --ipc host \
  --shm-size 1g \
  v2x-gpu
```

`--network host` is required for reliable ROS 2 DDS discovery between the container, host cameras, and host RViz. GPU access is fixed when the container is created; `docker start` cannot add `--gpus all` later.

Verify the runtime before launching detectors:

```bash
docker exec v2x-gpu-container nvidia-smi
docker exec v2x-gpu-container bash -lc \
  "ldd /ros2_ws/onnxruntime/lib/libonnxruntime_providers_cuda.so | grep 'not found' || true"
docker inspect --format '{{.State.Health.Status}}' v2x-gpu-container
```

The `ldd` command should print nothing and the health status should become `healthy`.

## Run Detectors

Detector launch files autostart lifecycle nodes by default, so manual `ros2 lifecycle set` commands are no longer required.

Example:

```bash
ros2 launch ros2_yolos_cpp detector2.launch.py \
  model_path:=/ros2_ws/src/V2x/ros2_yolos_cpp/yolov8n.onnx \
  labels_path:=/ros2_ws/src/V2x/ros2_yolos_cpp/coco.names \
  use_gpu:=true \
  image_topic:=/lucid_vision/camera_2/image
```

Useful launch arguments:

- `autostart:=true`: configure and activate automatically.
- `use_gpu:=true`: request the ONNX Runtime CUDA provider.
- `publish_debug_image:=false`: keep disabled for lowest latency.
- `publish_timing:=false`: enable only while profiling.
- `conf_threshold:=0.4`: detection confidence threshold.

For manual lifecycle control, launch with `autostart:=false`.

RViz does not render `vision_msgs/msg/Detection2DArray` bounding boxes directly. To view detections, launch detectors with `publish_debug_image:=true`, then load the annotated fusion image:

```bash
rviz2 -d "$(ros2 pkg prefix late_fusion_for_yolos_cpp)/share/late_fusion_for_yolos_cpp/config/fused_debug.rviz"
```

## Automated Multi-Camera Startup

### From The Host

Install tmuxinator, place `auto_start.yml` at `~/.tmuxinator/auto_start.yml`, then run:

```bash
tmuxinator start auto_start
```

This profile starts the persistent container, stops stale detector launch parents, staggers five GPU detector startups, and launches late fusion. Staggering reduces peak GPU initialization pressure.

### From Inside The Container

Use `auto_start2.yml` as an in-container three-camera tmuxinator profile.

## CUDA 702 Recovery And Prevention

When a detector sees CUDA error 702, CUDA error 700/719, a launch-timeout message, or `CUBLAS_STATUS_NOT_INITIALIZED`:

1. The adapter promotes it to a fatal inference error.
2. The affected detector process exits nonzero instead of silently dropping every future frame.
3. ROS launch respawns only that detector after one second.
4. The detector automatically configures and activates again with a fresh CUDA context.

To reduce the chance of recurrence:

- Keep debug images and timing disabled unless actively needed.
- Do not run multiple detectors in one process; process isolation is required for reliable CUDA-context recovery.
- Keep camera subscriptions at queue depth one.
- Stagger detector startup instead of initializing every GPU session simultaneously.
- Confirm the target NVIDIA driver reports CUDA 12.6 or newer and monitor GPU temperature, power, utilization, and memory with `nvidia-smi`.
- On systems with a GPU watchdog, configure the target as a compute workload or increase the watchdog timeout according to the operating system and NVIDIA driver policy.

A restart prevents permanent pipeline stalls, but repeated 702 errors still indicate that the target GPU workload or driver/watchdog configuration needs correction.

## Troubleshooting

```bash
# GPU and driver status
docker exec v2x-gpu-container nvidia-smi

# Missing ONNX Runtime CUDA dependencies
docker exec v2x-gpu-container bash -lc \
  "ldd /ros2_ws/onnxruntime/lib/libonnxruntime_providers_cuda.so | grep 'not found'"

# Confirm ROS topics are visible through host networking
docker exec v2x-gpu-container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 topic list"

# Inspect detector restart logs in the tmuxinator detector pane
tmux attach -t auto_start
```

## Visual Samples

<p align="center">
  <img src="late_fusion_for_yolos_cpp/assets/Fused_Rviz_view.gif" width="80%"/>
</p>
