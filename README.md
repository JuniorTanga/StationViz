# StationViz

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Qt](https://img.shields.io/badge/Qt-6.0%2B-green.svg)](https://www.qt.io/)
[![Standard](https://img.shields.io/badge/Standard-IEC%2061850-red.svg)](https://iec61850.com/)

**StationViz** is an open-source test tool designed to perform Factory Acceptance Tests (FAT) for IEC 61850 compliant digital substation control systems. It provides a free and powerful alternative to proprietary testing suites.

## Key Features

*   **SCL Parser**: Imports and analyzes SCL files (SCD, ICD, CID) to extract substation topology, IEDs, and data models.
*   **Interactive Single-Line Diagram (SLD)**: Automatically generates a real-time visual representation of the substation for intuitive monitoring.
*   **IEC 61850 Communication**: Supports MMS and GOOSE protocols to monitor IED data points and send control commands.

## Architecture

The project is structured into modular components for clear separation of concerns:
```bash
StationViz/
├── core/ # Backend libraries (C++)
│ ├── scl/ # SCL parsing & data model
│ ├── sld/ # Single-Line Diagram generation
│ └── network/ # IEC 61850 communication (MMS, GOOSE)
└── ui/ # Frontend application (Qt Quick / QML)
```

## Dependencies

*   **Qt 6**: Core, Quick, Xml modules
*   **libiec61850**: Core communication library (MMS/GOOSE)
*   **pugixml**: Lightweight XML parser for SCL files
*   **CMake** (3.16+): Build system

## Getting Started

### Prerequisites
Ensure you have Git, CMake, a C++ compiler, and Qt 6 installed.

### Build Instructions

```bash
# 1. Clone the repository
git clone https://github.com/JuniorTanga/StationViz.git
cd StationViz

# 2. Configure the project (example for a build directory)
cmake -B build -DCMAKE_PREFIX_PATH="path_to_your_Qt6" .

# 3. Build the project
cmake --build build

# 4. Run the application
./build/StationViz
```
## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details. It uses libiec61850, which is also GPLv3 licensed.

## Contributions
Contributions, issues, and feature requests are welcome! Feel free to check the issue page.

## Contact
Mail : tangajunior9@gmail.com
Repo : https://github.com/JuniorTanga/StationViz.git
