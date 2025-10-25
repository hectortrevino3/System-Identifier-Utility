<h1>System Identifier Management Utility</h1>
A tool written in C++ that allows for interaction with low-level Windows system identifiers for privacy research and development testing.

<img width="979" height="483" alt="image" src="https://github.com/user-attachments/assets/eb6737dc-df9d-4a98-bd50-978466522f78" />

<h1>About The Project</h1>
This is a utility for Windows that provides an interface for reading and modifying low-level system and hardware identifiers. I created this project to teach myself more about the Windows API and to better understand how software interacts with hardware on a fundamental level.

Disclaimer: This tool is intended for educational and research purposes only.

<h1>Key Features</h1>

- Windows API Interfacing: Directly interfaces with the Windows API to manage system processes, execute shell commands, and read from or write to the SMBIOS.

- Runtime Resource Extraction: Packages component files (like drivers and other programs) inside the main executable. At runtime, the tool automatically extracts and runs these hidden resources as needed.

- Secure API Authentication: Integrates a third-party authentication library (KeyAuth) to manage access to the tool's functions.

<h1>Built With</h1>

- C++

- Windows API
