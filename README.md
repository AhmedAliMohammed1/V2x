# Multi-Camera YOLO Detection and Late Fusion

<p align="center">
  <img src="late_fusion_for_yolos_cpp/assets/Banner.png" width="100%"/>
</p>

This project detects objects from multiple cameras and shows the results together
in one RViz window.

The default setup:

- Reads images from five camera topics.
- Runs one YOLO detector for each camera.
- Keeps only the object classes listed in a simple text file.
- Draws boxes and labels on the camera images.
- Combines the five annotated images into one panoramic view.
- Publishes the panoramic RViz view at about 30 FPS.
- Automatically restarts an individual detector after fatal CUDA errors.

This guide uses Docker with an NVIDIA GPU because it is the easiest and most
reliable way to run the complete project.

## Contents

1. [How the system works](#how-the-system-works)
2. [Before you start](#before-you-start)
3. [First-time setup](#first-time-setup)
4. [Daily startup](#daily-startup)
5. [Open RViz](#open-rviz)
6. [Choose which object classes are detected](#choose-which-object-classes-are-detected)
7. [Check that everything is working](#check-that-everything-is-working)
8. [Stop the system](#stop-the-system)
9. [Manual startup and custom settings](#manual-startup-and-custom-settings)
10. [Troubleshooting](#troubleshooting)
11. [Advanced source build](#advanced-source-build)

## How the System Works

The default pipeline uses these five image topics:

| Camera | Input image topic | Detection topic | Annotated image topic |
|---|---|---|---|
| Camera 1 | `/lucid_vision/camera_1/image` | `/yolos_detector1/detections` | `/yolos_detector1/debug_image` |
| Camera 2 | `/lucid_vision/camera_2/image` | `/yolos_detector2/detections` | `/yolos_detector2/debug_image` |
| Camera 3 | `/lucid_vision/camera_3/image` | `/yolos_detector3/detections` | `/yolos_detector3/debug_image` |
| Camera 4 | `/lucid_vision/camera_4/image` | `/yolos_detector4/detections` | `/yolos_detector4/debug_image` |
| Camera 5 | `/lucid_vision/camera_5/image` | `/yolos_detector5/detections` | `/yolos_detector5/debug_image` |

The late-fusion node combines those outputs:

| Output | Topic | Meaning |
|---|---|---|
| Fused detections | `/fused/detections` | Detection messages from all active cameras |
| Fused RViz image | `/fused/debug_image` | One panoramic image containing all annotated cameras |

RViz does not draw `vision_msgs/msg/Detection2DArray` messages directly. It
shows `/fused/debug_image`, where the detector has already drawn the boxes and
labels.

## Before You Start

### Required Software

The recommended GPU setup requires:

- Ubuntu or another Linux system that supports Docker.
- An NVIDIA GPU.
- An NVIDIA driver that supports CUDA 12.6 or newer.
- Docker.
- NVIDIA Container Toolkit.
- ROS 2 Jazzy and RViz on the host computer.
- `tmux` and `tmuxinator` on the host for the easiest startup method.

The Docker image provides CUDA, cuDNN, ONNX Runtime, OpenCV, ROS dependencies,
and the compiled project.

### Check the GPU and Docker

Run these commands on the host computer:

```bash
nvidia-smi
docker --version
docker info | grep -i runtime
```

`nvidia-smi` must show the NVIDIA GPU. Docker must be running before continuing.

### Check the Camera or ROS Bag Topics

The detectors need camera images. They can come from real cameras or a ROS bag.

If you use a ROS bag, inspect it first:

```bash
source /opt/ros/jazzy/setup.bash
ros2 bag info /path/to/your_bag
```

The default startup profile expects these topics:

```text
/lucid_vision/camera_1/image
/lucid_vision/camera_2/image
/lucid_vision/camera_3/image
/lucid_vision/camera_4/image
/lucid_vision/camera_5/image
```

If your topic names are different, change each `image_topic:=...` value in
`auto_start.yml` before starting the system.

## First-Time Setup

### 1. Download the Project

```bash
git clone https://github.com/AhmedAliMohammed1/V2x.git
cd V2x
```

All commands below assume the terminal is inside the `V2x` project directory.

### 2. Build the Docker Image

The first build downloads the required dependencies and can take several
minutes:

```bash
docker build -t ros2_yolos_cpp:latest .
```

### 3. Create the Persistent GPU Container

Create the container once:

```bash
docker run -d \
  --name ros2_yolos_gpu_container \
  --restart unless-stopped \
  --gpus all \
  --shm-size 1g \
  ros2_yolos_cpp:latest
```

Important:

- `--gpus all` gives the container access to the NVIDIA GPU.


If a container with the same name already exists, start it instead:

```bash
docker start ros2_yolos_gpu_container
```

### 4. Verify the Container

```bash
docker ps --filter name=ros2_yolos_gpu_container
docker exec ros2_yolos_gpu_container nvidia-smi
docker inspect --format '{{.State.Health.Status}}' ros2_yolos_gpu_container
docker exec ros2_yolos_gpu_container bash -lc \
  "ldd /ros2_ws/onnxruntime/lib/libonnxruntime_providers_cuda.so | grep 'not found' || true"
```

Expected results:

- The container status is `Up`.
- `nvidia-smi` shows the GPU.
- The health status becomes `healthy`.
- The `ldd` command prints nothing. Any `not found` line means a required CUDA
  library is missing.

### 5. Install the Easy Startup Tool

Install `tmuxinator` on the host:

```bash
sudo apt update
sudo apt install -y tmux ruby-full
sudo gem install tmuxinator
mkdir -p ~/.tmuxinator
cp "$PWD/auto_start.yml" ~/.config/tmuxinator/auto_start.yml
```

You only need to copy the profile again after changing `auto_start.yml`.

## Daily Startup

Use three terminals on the host computer.

### Terminal 1: Start the Cameras or ROS Bag

For a ROS bag:

```bash
source /opt/ros/jazzy/setup.bash
ros2 bag play /path/to/your_bag --loop
```

Keep this terminal running.

### Terminal 2: Start All Five Detectors and Late Fusion

From the project directory:

```bash
tmuxinator start auto_start
```

The profile:

1. Starts `ros2_yolos_gpu_container`.
2. Stops stale detector processes from an older run.
3. Starts the five GPU detectors one at a time.
4. Enables annotated debug images for RViz.
5. Starts the late-fusion node.

Detector startup is staggered to reduce GPU initialization problems.

Useful tmux command:

```text
Ctrl+b, then d     Leave the session running in the background
```

Reconnect later:

```bash
tmux attach -t auto_start
```

### Terminal 3: Open RViz

Follow the next section.

## Open RViz

RViz should run on the host computer, not inside the Docker container.

Install RViz if needed:

```bash
sudo apt update
sudo apt install -y ros-jazzy-rviz2
```

Open the prepared fused-image configuration from the project directory:

```bash
source /opt/ros/jazzy/setup.bash
rviz2 -d "$PWD/late_fusion_for_yolos_cpp/config/fused_debug.rviz"
```

RViz should subscribe to `/fused/debug_image` and show five annotated camera
tiles in one row.

The fused image is published at about 30 FPS. Camera and detector rates may be
lower, so RViz can repeat the latest annotated frame between new detections.

## Choose Which Object Classes Are Detected

### Simple Explanation

The detector has a text file that acts like a guest list:

- A class written in the file is kept.
- A class missing from the file is removed.

The editable source file is:

```text
ros2_yolos_cpp/config/allowed_classes.txt
```

The default file contains:

```text
car
truck
bus
person
traffic light
```

Class names are exact and case-sensitive. Use names from:

```text
ros2_yolos_cpp/coco.names
```

For example, `traffic light` is valid, but `Traffic Light` is not.

### File Rules

- Write one class name per line.
- Blank lines are ignored.
- Lines beginning with `#` are comments and are ignored.
- Duplicate class names are ignored.
- Do not leave the file empty.
- Every class must exist in `coco.names`.

If the file is missing, empty, or contains an unknown class, the detector safely
refuses to start and prints a clear error.

### Example: Detect Only Vehicles

Edit the file:

```bash
nano ros2_yolos_cpp/config/allowed_classes.txt
```

Use:

```text
# Vehicles only
car
truck
bus
```

### Example: Detect Only People

```text
person
```

### Example: Keep Every COCO Class

Copy all class names into the allowlist:

```bash
cp ros2_yolos_cpp/coco.names ros2_yolos_cpp/config/allowed_classes.txt
```

An empty file does not mean "keep everything". An empty file is treated as an
error.

### Apply a Class Change Quickly to the Current Container

This is the fastest method and does not rebuild the Docker image.

From the project directory:

```bash
docker cp \
  ros2_yolos_cpp/config/allowed_classes.txt \
  ros2_yolos_gpu_container:/ros2_ws/install/ros2_yolos_cpp/share/ros2_yolos_cpp/config/allowed_classes.txt
```

Reload the file by reconfiguring the five detectors one at a time:

```bash
docker exec ros2_yolos_gpu_container bash -lc '
source /opt/ros/jazzy/setup.bash
source /ros2_ws/install/setup.bash

for n in 1 2 3 4 5; do
  echo "Reloading detector $n"
  ros2 lifecycle set /yolos_detector$n deactivate
  ros2 lifecycle set /yolos_detector$n cleanup
  ros2 lifecycle set /yolos_detector$n configure
  ros2 lifecycle set /yolos_detector$n activate
  sleep 2
done
'
```

This change remains in the current container, but it is lost if the container is
deleted and recreated.

### Make a Class Change Permanent in a New Docker Image

After editing `ros2_yolos_cpp/config/allowed_classes.txt`, rebuild the image:

```bash
docker build -t ros2_yolos_cpp:latest .
```

Stop the current startup session, replace the container, and start again:

```bash
tmux kill-session -t auto_start 2>/dev/null || true
docker rm -f ros2_yolos_gpu_container

docker run -d \
  --name ros2_yolos_gpu_container \
  --restart unless-stopped \
  --gpus all \
  --network host \
  --ipc host \
  --shm-size 1g \
  ros2_yolos_cpp:latest

tmuxinator start auto_start
```

## Check That Everything Is Working

The commands in this section run on the host and query ROS 2 inside the
container.

### Check the Camera Inputs

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 topic list | grep /lucid_vision"
```

You should see the expected camera image topics.

### Check Detector Lifecycle States

```bash
docker exec -it ros2_yolos_gpu_container bash -lc '
source /opt/ros/jazzy/setup.bash
source /ros2_ws/install/setup.bash

for n in 1 2 3 4 5; do
  printf "Detector %s: " "$n"
  ros2 lifecycle get /yolos_detector$n
done
'
```

Every detector should report:

```text
active [3]
```

### Check Detection Rates

Run one command at a time. Press `Ctrl+C` after several seconds:

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 topic hz /yolos_detector1/detections"
```

Change `detector1` to `detector2`, `detector3`, `detector4`, or `detector5` to
check another camera.

### Check the RViz Fused Image Rate

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 topic hz /fused/debug_image"
```

The configured fused-image rate is approximately 30 FPS.

### View One Detection Message

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 topic echo --once /fused/detections"
```

### Check Which Allowlist File Is Active

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 param get /yolos_detector1 allowed_classes_path"
```

The default installed path is:

```text
/ros2_ws/install/ros2_yolos_cpp/share/ros2_yolos_cpp/config/allowed_classes.txt
```

## Stop the System

Stop the tmux startup session:

```bash
tmux kill-session -t auto_start 2>/dev/null || true
```

Stop any remaining detector and fusion processes:

```bash
docker exec ros2_yolos_gpu_container bash -lc \
  "pkill -f '[r]os2 launch ros2_yolos_cpp detector' || true; \
   pkill -f '[r]os2 launch late_fusion_for_yolos_cpp' || true; \
   pkill -f '[y]olos_detector_node( |$)' || true; \
   pkill -f '[l]ate_fusion_node( |$)' || true"
```

Stop the persistent container when it is no longer needed:

```bash
docker stop ros2_yolos_gpu_container
```

## Manual Startup and Custom Settings

Use manual startup when testing one detector or when `tmuxinator` is not
available. Do not run the same detector manually while `auto_start` is active.

### Start One Detector

This example starts camera 1:

```bash
docker exec -it ros2_yolos_gpu_container bash -lc "
source /opt/ros/jazzy/setup.bash
source /ros2_ws/install/setup.bash

ros2 launch ros2_yolos_cpp detector1.launch.py \
  model_path:=/ros2_ws/src/V2x/ros2_yolos_cpp/yolov8n.onnx \
  labels_path:=/ros2_ws/src/V2x/ros2_yolos_cpp/coco.names \
  use_gpu:=true \
  publish_debug_image:=true \
  image_topic:=/lucid_vision/camera_1/image
"
```

### Start Late Fusion

Run this in another terminal after starting the required detectors:

```bash
docker exec -it ros2_yolos_gpu_container bash -lc "
source /opt/ros/jazzy/setup.bash
source /ros2_ws/install/setup.bash
ros2 launch late_fusion_for_yolos_cpp launch_fusion_node.py
"
```

### Useful Detector Settings

| Setting | Example | Meaning |
|---|---|---|
| `use_gpu` | `use_gpu:=true` | Use NVIDIA GPU inference |
| `publish_debug_image` | `publish_debug_image:=true` | Publish annotated images required by the fused RViz view |
| `publish_timing` | `publish_timing:=true` | Publish timing data for profiling |
| `conf_threshold` | `conf_threshold:=0.4` | Ignore detections below this confidence |
| `allowed_classes_path` | `allowed_classes_path:=/path/inside/container/classes.txt` | Use a custom class-filter file |
| `image_topic` | `image_topic:=/camera/image` | Choose the input camera topic |
| `autostart` | `autostart:=true` | Configure and activate the lifecycle node automatically |

Paths passed to a detector running in Docker must exist inside the container.
Use `docker cp` to copy a custom file into the container first.

## Troubleshooting

### The Detector Prints Results but RViz Is Empty

Check that annotated images are enabled and published:

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 topic info /yolos_detector1/debug_image"
```

`Publisher count` should be at least `1`.

Check the fused image:

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 topic info /fused/debug_image"
```

Expected:

- `Publisher count: 1`
- `Subscription count: 1` while RViz is open

Open RViz with the supplied configuration:

```bash
source /opt/ros/jazzy/setup.bash
rviz2 -d "$PWD/late_fusion_for_yolos_cpp/config/fused_debug.rviz"
```

### A Detector Is Not Active

```bash
docker exec -it ros2_yolos_gpu_container bash -lc \
  "source /opt/ros/jazzy/setup.bash && source /ros2_ws/install/setup.bash && ros2 lifecycle get /yolos_detector1"
```

Inspect the detector logs:

```bash
tmux attach -t auto_start
```

### The Allowlist File Is Rejected

Common causes:

- The file path does not exist inside the container.
- The file is empty.
- A class has the wrong capitalization.
- A class is not present in `ros2_yolos_cpp/coco.names`.
- A multi-word class such as `traffic light` was split into separate lines.

View the installed file:

```bash
docker exec ros2_yolos_gpu_container cat \
  /ros2_ws/install/ros2_yolos_cpp/share/ros2_yolos_cpp/config/allowed_classes.txt
```

### Camera Topics Are Missing

Confirm the ROS bag or cameras are running:

```bash
source /opt/ros/jazzy/setup.bash
ros2 topic list | grep /lucid_vision
```

If topics are visible on the host but not in Docker, recreate the container with
`--network host`.

### CUDA or GPU Error

Check the GPU:

```bash
docker exec ros2_yolos_gpu_container nvidia-smi
```

Check ONNX Runtime CUDA dependencies:

```bash
docker exec ros2_yolos_gpu_container bash -lc \
  "ldd /ros2_ws/onnxruntime/lib/libonnxruntime_providers_cuda.so | grep 'not found' || true"
```

The dependency command should print nothing.

Fatal CUDA errors such as CUDA 700, 702, 719, launch timeout, or
`CUBLAS_STATUS_NOT_INITIALIZED` cause only the affected detector process to exit.
Its ROS launch supervisor then creates and activates a fresh detector process.

Repeated CUDA failures can indicate GPU overload, high temperature, driver
problems, or an operating-system GPU watchdog.


### Repair NVIDIA Drivers, CUDA Libraries, or Missing GPU Libraries

Use this section only when the checks above report a driver error, Docker cannot
access the GPU, or `ldd` reports a missing library.

Important:

- For the recommended Docker setup, the host computer needs the NVIDIA driver
  and NVIDIA Container Toolkit.
- The project Docker image provides CUDA 12.6 runtime libraries, cuDNN 9, and
  ONNX Runtime GPU 1.20.1.
- Do not install a complete CUDA toolkit on the host just to run this Docker
  project.
- Do not manually delete `.so` library files. Repair or reinstall their package.
- The commands below are for Ubuntu. Driver removal can temporarily disable the
  desktop display, so save your work before starting.

#### Find Which Layer Is Broken

Run these checks in order.

1. Check the driver on the host:

```bash
nvidia-smi
```

2. Check whether Docker can use the host GPU:

```bash
docker run --rm --gpus all ubuntu:24.04 nvidia-smi
```

3. Check the CUDA libraries inside the project container:

```bash
docker exec ros2_yolos_gpu_container bash -lc \
  "ldd /ros2_ws/onnxruntime/lib/libonnxruntime_providers_cuda.so | grep 'not found' || true"
```

Use the result to choose the correct repair:

| Result | Broken layer | Repair |
|---|---|---|
| Host `nvidia-smi` fails | NVIDIA host driver | Repair or reinstall the driver |
| Host works, but the Docker GPU test fails | NVIDIA Container Toolkit | Reinstall and configure the toolkit |
| Docker GPU test works, but project `ldd` shows `not found` | Project image CUDA libraries | Rebuild or repair the project container |
| Every check passes, but CUDA errors continue | Runtime load, temperature, driver stability, or GPU watchdog | Check GPU usage, temperature, and logs |

Useful host diagnostics:

```bash
hostnamectl
lspci | grep -i nvidia
lsmod | grep nvidia
cat /proc/driver/nvidia/version
apt-mark showmanual | grep -E 'nvidia|cuda'
sudo dmesg | grep -Ei 'nvrm|nvidia|xid'
```

#### Repair the NVIDIA Host Driver

First, try a reboot. This often fixes:

```text
Failed to initialize NVML: Driver/library version mismatch
```

```bash
sudo reboot
```

If `nvidia-smi` still fails after reboot, update the package information,
install the current kernel headers, and let Ubuntu select the recommended
compute driver:

```bash
sudo apt update
sudo apt install --reinstall -y linux-headers-$(uname -r)
sudo ubuntu-drivers list --gpgpu
sudo ubuntu-drivers install --gpgpu
sudo reboot
```

For a normal desktop workstation, use this instead of the `--gpgpu` install
command:

```bash
sudo ubuntu-drivers install
```

After rebooting:

```bash
nvidia-smi
```

The driver shown by `nvidia-smi` must support CUDA 12.6 or newer. The
`ubuntu-drivers` command is preferred because it selects a driver supported by
the installed GPU and Ubuntu version.

#### Completely Remove and Reinstall a Broken NVIDIA Driver

Only use this when the normal driver repair did not work.

First, display the installed packages and available driver branches:

```bash
apt-mark showmanual | grep nvidia
sudo ubuntu-drivers list --gpgpu
```

Set `DRIVER_BRANCH` to an installed broken branch. The following uses `570` only
as an example:

```bash
export DRIVER_BRANCH=570
sudo apt --purge remove "*nvidia*${DRIVER_BRANCH}*"
sudo apt autoremove --purge -y
sudo apt update
sudo ubuntu-drivers install --gpgpu
sudo reboot
```

Use `sudo ubuntu-drivers install` instead of `--gpgpu` for a normal desktop
driver. If multiple old driver branches are installed, remove each old branch
one at a time.

After rebooting, verify:

```bash
nvidia-smi
cat /proc/driver/nvidia/version
lsmod | grep nvidia
```

If Secure Boot is enabled, use Ubuntu's signed recommended driver and complete
any MOK key enrollment screen shown during reboot:

```bash
mokutil --sb-state
```

If `nvidia-smi` says `No devices were found`, check whether the conflicting
`nouveau` driver is loaded:

```bash
lsmod | grep nouveau
```

Only if that command prints a `nouveau` module, disable it and reboot:

```bash
echo "blacklist nouveau" | sudo tee /etc/modprobe.d/disable-nouveau.conf
echo "options nouveau modeset=0" | sudo tee -a /etc/modprobe.d/disable-nouveau.conf
sudo update-initramfs -u
sudo reboot
```

#### Reinstall the NVIDIA Container Toolkit

Use this when host `nvidia-smi` works but this command fails:

```bash
docker run --rm --gpus all ubuntu:24.04 nvidia-smi
```

Install the repository prerequisites and configure NVIDIA's production
repository:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends ca-certificates curl gnupg2

curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo gpg --dearmor --yes \
  -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg

curl -s -L \
  https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
  sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list
```

Reinstall and configure the toolkit:

```bash
sudo apt-get update
sudo apt-get install --reinstall -y \
  nvidia-container-toolkit \
  nvidia-container-toolkit-base \
  libnvidia-container-tools \
  libnvidia-container1

sudo nvidia-ctk runtime configure --runtime=docker
sudo systemctl restart docker
```

Verify Docker GPU access before restarting this project:

```bash
docker run --rm --gpus all ubuntu:24.04 nvidia-smi
docker start ros2_yolos_gpu_container
docker exec ros2_yolos_gpu_container nvidia-smi
```

#### Find and Repair Missing CUDA Libraries Inside the Project Container

List every missing ONNX Runtime GPU dependency:

```bash
docker exec ros2_yolos_gpu_container bash -lc \
  "ldd /ros2_ws/onnxruntime/lib/libonnxruntime_providers_cuda.so | grep 'not found' || true"
```

Common missing files and the packages that provide them:

| Missing library example | Required package in this project |
|---|---|
| `libcudart.so.12` | `cuda-cudart-12-6` |
| `libnvrtc.so.12` | `cuda-nvrtc-12-6` |
| `libcublas.so.12` or `libcublasLt.so.12` | `libcublas-12-6` |
| `libcufft.so.11` | `libcufft-12-6` |
| `libcurand.so.10` | `libcurand-12-6` |
| `libcudnn.so.9` | `libcudnn9-cuda-12` |
| `libonnxruntime_providers_cuda.so` | ONNX Runtime GPU 1.20.1 |

The safest permanent repair is to rebuild the image. The Docker build verifies
these libraries before compiling the project:

```bash
tmux kill-session -t auto_start 2>/dev/null || true
docker rm -f ros2_yolos_gpu_container 2>/dev/null || true
docker build --no-cache -t ros2_yolos_cpp:latest .

docker run -d \
  --name ros2_yolos_gpu_container \
  --restart unless-stopped \
  --gpus all \
  --network host \
  --ipc host \
  --shm-size 1g \
  ros2_yolos_cpp:latest
```

For a quick temporary repair of the current container:

```bash
docker exec -it ros2_yolos_gpu_container bash -lc '
apt-get update
apt-get install --reinstall -y --no-install-recommends \
  cuda-cudart-12-6 \
  cuda-nvrtc-12-6 \
  libcublas-12-6 \
  libcufft-12-6 \
  libcurand-12-6 \
  libcudnn9-cuda-12
ldconfig
ldd /ros2_ws/onnxruntime/lib/libonnxruntime_providers_cuda.so
'
```

This temporary repair is lost when the container is deleted. Rebuild the image
to make it permanent.

#### Repair CUDA Libraries on the Host for a Source Build

Skip this section when using Docker. It is only for users running the project
directly on an Ubuntu 24.04 host.

Add the CUDA repository used by this project and reinstall the exact runtime
packages:

```bash
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
rm cuda-keyring_1.1-1_all.deb

sudo apt update
sudo apt install --reinstall -y --no-install-recommends \
  cuda-cudart-12-6 \
  cuda-nvrtc-12-6 \
  libcublas-12-6 \
  libcufft-12-6 \
  libcurand-12-6 \
  libcudnn9-cuda-12
sudo ldconfig
```

To completely remove and reinstall only those CUDA runtime packages:

```bash
sudo apt --purge remove -y \
  cuda-cudart-12-6 \
  cuda-nvrtc-12-6 \
  libcublas-12-6 \
  libcufft-12-6 \
  libcurand-12-6 \
  libcudnn9-cuda-12
sudo apt autoremove --purge -y

sudo apt update
sudo apt install -y --no-install-recommends \
  cuda-cudart-12-6 \
  cuda-nvrtc-12-6 \
  libcublas-12-6 \
  libcufft-12-6 \
  libcurand-12-6 \
  libcudnn9-cuda-12
sudo ldconfig
```

If APT reports broken or unfinished package installation, repair APT before
reinstalling the libraries:

```bash
sudo dpkg --configure -a
sudo apt --fix-broken install
sudo apt update
```

If `libonnxruntime_providers_cuda.so` itself is missing during a host source
build, download the same ONNX Runtime GPU release used by the Docker image:

```bash
export ONNXRUNTIME_VERSION=1.20.1
export ONNXRUNTIME_DIR="$HOME/onnxruntime-gpu-$ONNXRUNTIME_VERSION"

mv "$ONNXRUNTIME_DIR" "$ONNXRUNTIME_DIR.backup.$(date +%s)" 2>/dev/null || true
mkdir -p "$ONNXRUNTIME_DIR"
wget "https://github.com/microsoft/onnxruntime/releases/download/v$ONNXRUNTIME_VERSION/onnxruntime-linux-x64-gpu-$ONNXRUNTIME_VERSION.tgz" \
  -O /tmp/onnxruntime-gpu.tgz
tar -xzf /tmp/onnxruntime-gpu.tgz -C "$ONNXRUNTIME_DIR" --strip-components=1
rm /tmp/onnxruntime-gpu.tgz

find "$ONNXRUNTIME_DIR" -name 'libonnxruntime_providers_cuda.so'
ldd "$ONNXRUNTIME_DIR/lib/libonnxruntime_providers_cuda.so" | \
  grep 'not found' || true
```

The final `ldd` command should print nothing. Use this `ONNXRUNTIME_DIR` value
when running `colcon build`.

Verify the dynamic library cache and the ONNX Runtime GPU provider:

```bash
ldconfig -p | grep -E 'cudart|nvrtc|cublas|cufft|curand|cudnn'
ldd "$ONNXRUNTIME_DIR/lib/libonnxruntime_providers_cuda.so" | \
  grep 'not found' || true
```

The final `ldd` command should print nothing.

Official references:

- [Ubuntu NVIDIA driver installation](https://ubuntu.com/server/docs/how-to/graphics/install-nvidia-drivers/)
- [NVIDIA Container Toolkit installation](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)
- [NVIDIA CUDA installation guide](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html)

### Rebuild and Recreate Everything

Use this when the image or container configuration is damaged:

```bash
tmux kill-session -t auto_start 2>/dev/null || true
docker rm -f ros2_yolos_gpu_container 2>/dev/null || true
docker build --no-cache -t ros2_yolos_cpp:latest .

docker run -d \
  --name ros2_yolos_gpu_container \
  --restart unless-stopped \
  --gpus all \
  --network host \
  --ipc host \
  --shm-size 1g \
  ros2_yolos_cpp:latest
```

## Advanced Source Build

Docker is recommended for GPU operation. Build directly on the host only when
you understand ROS workspaces and ONNX Runtime installation.

Create a ROS workspace:

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
git clone https://github.com/AhmedAliMohammed1/V2x.git
cd ~/ros2_ws

source /opt/ros/jazzy/setup.bash
rosdep update
rosdep install --from-paths src --ignore-src -r -y
```

Set `ONNXRUNTIME_DIR` to an ONNX Runtime installation containing `include/` and
`lib/`.

CPU build:

```bash
export ONNXRUNTIME_DIR=/opt/onnxruntime

colcon build --cmake-args \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_GPU=OFF \
  -DONNXRUNTIME_DIR="$ONNXRUNTIME_DIR"

source install/setup.bash
```

GPU-validated build:

```bash
export ONNXRUNTIME_DIR=/opt/onnxruntime-gpu

colcon build --cmake-args \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_GPU=ON \
  -DONNXRUNTIME_DIR="$ONNXRUNTIME_DIR"

source install/setup.bash
```

Run package tests inside the prepared Docker environment:

```bash
docker exec -it ros2_yolos_gpu_container bash -lc '
source /opt/ros/jazzy/setup.bash
cd /ros2_ws

colcon build --packages-select ros2_yolos_cpp --cmake-args -DBUILD_TESTING=ON
colcon test --packages-select ros2_yolos_cpp
colcon test-result --verbose --test-result-base build/ros2_yolos_cpp
'
```

## Visual Sample

<p align="center">
  <img src="late_fusion_for_yolos_cpp/assets/Fused_Rviz_view.gif" width="80%"/>
</p>
