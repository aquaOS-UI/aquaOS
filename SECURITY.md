# Security Policy

The AquaOS team takes the security and stability of our software and its users very seriously. As a desktop environment layer interacting directly with Windows Desktop Window Manager (DWM), Win32 shell hooks, and hardware controller APIs, we adhere to strict security and safety standards.

---

## 🛡️ Supported Versions

We provide security updates and patches for the following versions:

| Version | Supported | Notes |
| :--- | :---: | :--- |
| **v2.x (Current)** | :white_check_mark: | Active development & security patches |
| **v0.0.1** | :white_check_mark: | Initial baseline release |
| **< v0.0.1** | :x: | Unsupported prototype builds |

---

## 🔒 Reporting a Vulnerability

If you discover a security vulnerability, security flaw, or potential privilege escalation in AquaOS, **please do not open a public issue.**

Instead, please report it through one of the following coordinated disclosure channels:

1. **GitHub Private Vulnerability Reporting:**
   Navigate to the **Security** tab of this repository and click **"Report a vulnerability"** to open a private advisory.

2. **Security Contact:**
   Send an email to `security@aquashell.dev` (or the repository maintainers) with the subject:
   `[SECURITY] Vulnerability Report: AquaOS`

### Information to Include
To help us triage and resolve the issue quickly, please provide:
- A clear description of the vulnerability.
- Affected versions and operating system build (Windows 10/11 version).
- Step-by-step instructions or proof-of-concept (PoC) code to reproduce the issue.
- Potential impact (e.g., denial of service, memory corruption, privilege boundary crossing).
- Any proposed mitigations or fixes.

---

## ⏱️ Response & Disclosure Timeline

- **Initial Response:** We will acknowledge receipt of your report within **48 hours**.
- **Assessment & Triage:** We will investigate and provide a severity assessment within **5 business days**.
- **Fix & Disclosure:** We will work on a patch and coordinate a release date. Once a fix is released, we will publicly disclose the vulnerability and credit the researcher.

---

## ⚙️ Security & Safety Architecture in AquaOS

AquaOS is architected with several defensive design measures:

1. **Least Privilege by Default:**
   - AquaOS runs in standard user context and does not require administrative privileges for daily operation.
   - The installer supports non-elevated per-user installation.

2. **Fail-Safe Shell Restoration:**
   - AquaOS registers multiple crash and termination handlers (`atexit`, `SetUnhandledExceptionFilter`, `SetConsoleCtrlHandler`) to guarantee the Windows taskbar and work area are restored if the process exits unexpectedly.
   - A standalone recovery executable (`RestoreTaskbar.exe`) and batch script (`scripts/restore.bat`) are packaged with every installation.

3. **Safe Memory & COM Handling:**
   - Memory management uses modern C++20 RAII patterns, smart pointers, and `Microsoft::WRL::ComPtr<T>` to prevent buffer overflows, use-after-free, and resource leaks.

4. **Exception-Guarded WinRT Integrations:**
   - All asynchronous WinRT interactions (media controls, toast listeners, hardware queries) are isolated in dedicated worker threads and wrapped in exception boundaries to prevent host process termination.
