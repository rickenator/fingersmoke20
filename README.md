# Fingersmoke20

Fingersmoke20 is a high-performance Vulkan-based rendering application designed for Android. It leverages Vulkan's advanced graphics capabilities to deliver real-time visual effects, focusing on efficient GPU utilization to handle complex rendering tasks. Fingersmoke20 implements CFD using a Navier-Stokes solution to simulate a smoke effect that emanates from the finger tip, as if finger painting with smoke on your Android screen. In particular, I am mostly interested in the Vulkan compute pipeline for this app.

This is a reboot of a project from the early days of Android that used GLES but no shaders that I had in the Google app market.


This is a Work In Progress (WIP) as such it is currently in bootstrap mode.

## Features

- Real-time Vulkan graphics rendering
- Support for multi-threading to optimize graphics processing
- Customizable rendering pipelines
- Efficient management of GPU resources and synchronization

## Getting Started

### Prerequisites

- Android API level 29 or higher
- Support for Vulkan API on the device

### Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/fingersmoke20.git
   ```
2. Open the project with Android Studio or your preferred IDE.
3. Build the project and deploy it to a compatible Android device.

### Usage

Launch the app on your device. Interactions and graphics adjustments can be made through the touchscreen interface.

## Contributing

Contributions are welcome! Please fork the repository and submit pull requests with your proposed changes.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.
