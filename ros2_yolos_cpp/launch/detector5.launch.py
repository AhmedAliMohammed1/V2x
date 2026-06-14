# Copyright 2024 YOLOs-CPP Team
# SPDX-License-Identifier: AGPL-3.0

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode


def generate_launch_description():
    """Launch a standalone YOLO detector lifecycle node."""
    model_path_arg = DeclareLaunchArgument(
        'model_path',
        description='Path to ONNX model file'
    )
    labels_path_arg = DeclareLaunchArgument(
        'labels_path',
        description='Path to class names file'
    )
    use_gpu_arg = DeclareLaunchArgument(
        'use_gpu',
        default_value='false',
        description='Enable GPU inference'
    )
    autostart_arg = DeclareLaunchArgument(
        'autostart',
        default_value='true',
        description='Automatically configure and activate the lifecycle detector'
    )
    publish_debug_image_arg = DeclareLaunchArgument(
        'publish_debug_image',
        default_value='false',
        description='Publish annotated images; disabled for minimum latency'
    )
    publish_timing_arg = DeclareLaunchArgument(
        'publish_timing',
        default_value='false',
        description='Publish per-frame timing metrics'
    )
    conf_threshold_arg = DeclareLaunchArgument(
        'conf_threshold',
        default_value='0.4',
        description='Confidence threshold'
    )
    image_topic_arg = DeclareLaunchArgument(
        'image_topic',
        default_value='/camera/image_raw',
        description='Input image topic'
    )
    camera_info_topic_arg = DeclareLaunchArgument(
        'camera_info_topic',
        default_value='/camera/camera_info',
        description='Camera info topic'
    )

    # Direct process respawn recreates the node and its CUDA context together.
    detector = LifecycleNode(
        package='ros2_yolos_cpp',
        executable='yolos_detector_node',
        name='yolos_detector5',
        namespace='',
        output='screen',
        parameters=[{
            'model_path': LaunchConfiguration('model_path'),
            'labels_path': LaunchConfiguration('labels_path'),
            'autostart': LaunchConfiguration('autostart'),
            'use_gpu': LaunchConfiguration('use_gpu'),
            'conf_threshold': LaunchConfiguration('conf_threshold'),
            'nms_threshold': 0.45,
            'publish_debug_image': LaunchConfiguration('publish_debug_image'),
            'publish_timing': LaunchConfiguration('publish_timing'),
        }],
        remappings=[
            ('~/image_raw', LaunchConfiguration('image_topic')),
            ('~/camera_info', LaunchConfiguration('camera_info_topic')),
        ],
        respawn=True,
        respawn_delay=1.0,
        respawn_max_retries=-1,
    )

    return LaunchDescription([
        model_path_arg,
        labels_path_arg,
        use_gpu_arg,
        autostart_arg,
        publish_debug_image_arg,
        publish_timing_arg,
        conf_threshold_arg,
        image_topic_arg,
        camera_info_topic_arg,
        detector,
    ])
