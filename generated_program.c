
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
extern char **environ;

int main() {
    char *args[] = {
        "/bin/bash", "-c",
        "#!/bin/bash\n"\
"\n"\
"# Start logging\n"\
"echo \"===== Starting System Validation =====\"\n"\
"echo \"Timestamp: $(date)\"\n"\
"echo \"======================================\"\n"\
"\n"\
"# Log file\n"\
"LOG_FILE=\"aiDAPTIV_Middleware_Deployment.log\"\n"\
"\n"\
"# Redirect stdout and stderr to the log file\n"\
"if ! exec > >(tee -a \"$LOG_FILE\") 2>&1; then\n"\
"    echo \"Permission denied: Unable to create or write to the log file $LOG_FILE. Please check the file permissions.\"\n"\
"    exit 1\n"\
"fi\n"\
"\n"\
"# Append env paths\n"\
"appenvpath () {\n"\
"\n"\
"    case \":$PATH:\" in\n"\
"        *:\"$1\":*)\n"\
"            echo 'ENV path exists'\n"\
"            ;;\n"\
"        *)\n"\
"            echo 'export PATH=\"/home/$USER/.local/bin:$PATH\"' >> ~/.bashrc\n"\
"            echo 'added ENV path to ~/.bashrc'\n"\
"    esac\n"\
"}\n"\
"\n"\
"# Function to check available disk space\n"\
"check_space() {\n"\
"    local required_space=$1\n"\
"    required_bytes=$(numfmt --from=iec $required_space)\n"\
"    available_space=$(df -h / | awk 'NR==2 {print $4}')\n"\
"    available_bytes=$(df --block-size=1 / | awk 'NR==2 {print $4}')\n"\
"    if [[ $available_bytes -ge $required_bytes ]]; then\n"\
"        echo \"✔ Sufficient space available. Required: $required_space, Available: $available_space\"\n"\
"        return 0\n"\
"    else\n"\
"        echo \"✘ Insufficient space. Required: $required_space, Available: $available_space\"\n"\
"        return 1\n"\
"    fi\n"\
"}\n"\
"\n"\
"# Function to check Python3 installation\n"\
"check_python3() {\n"\
"    PYTHON_PATH=$(which python3 | xargs dirname)\n"\
"    if [ -z \"$PYTHON_PATH\" ]; then\n"\
"        echo \"✘ Python3 is not installed or not found in the PATH.\"\n"\
"        return 1\n"\
"    else\n"\
"        echo \"✔ Python3 is installed at: $PYTHON_PATH\"\n"\
"        return 0\n"\
"    fi\n"\
"}\n"\
"\n"\
"# Function to check the package manager\n"\
"check_package_manager() {\n"\
"    if sudo apt-get update &> /dev/null; then\n"\
"        echo \"✔ Package manager (apt) is functioning correctly.\"\n"\
"        return 0\n"\
"    else\n"\
"        echo \"✘ Unable to update package lists. Problem with apt-get.\"\n"\
"        return 1\n"\
"    fi\n"\
"}\n"\
"\n"\
"\n"\
"remove_nvidia_drivers() {\n"\
"    # Show a temporary pop-up dialog for 3 seconds\n"\
"    (\n"\
"        yad --title=\"Removing NVIDIA Drivers\" \\\n"\
"            --width=400 --height=150 \\\n"\
"            --text-align=center \\\n"\
"            --no-buttons \\\n"\
"            --text=\"<b>Removing NVIDIA drivers, please wait...</b>\" \\\n"\
"            --timeout=3 \\\n"\
"            --timeout-indicator=top \\\n"\
"            --center\n"\
"    ) &\n"\
"\n"\
"    echo \"🔄 Starting NVIDIA driver removal process...\"\n"\
"    \n"\
"    # Remove NVIDIA packages\n"\
"    echo \"Removing NVIDIA packages...\"\n"\
"    sudo apt-get remove -y --purge '^nvidia-.*'\n"\
"\n"\
"    # Remove CUDA and related libraries\n"\
"    echo \"Removing CUDA and related libraries...\"\n"\
"    CUDA_PACKAGES=$(dpkg -l | grep 'cuda-toolkit-' | awk '{print $2}')\n"\
"    for package in $CUDA_PACKAGES; do\n"\
"        sudo apt-get remove -y \"$package\"\n"\
"    done\n"\
"    \n"\
"    # Cleanup\n"\
"    echo \"Cleaning up residual packages...\"\n"\
"    sudo apt-get remove -y --purge cuda* libnvidia* nvidia-settings nvidia-prime\n"\
"    sudo apt-get autoremove -y\n"\
"    sudo apt-get autoclean -y\n"\
"    sudo rm -rf /usr/local/cuda*\n"\
"    sudo rm -rf /usr/share/nvidia*\n"\
"    sed -i '/cuda/d' ~/.bashrc\n"\
"    source ~/.bashrc\n"\
"\n"\
"    # Final cleanup\n"\
"    echo \"Removing remaining NVIDIA configuration files...\"\n"\
"    sudo rm -rf /usr/local/cuda*\n"\
"    sudo rm -rf /usr/share/nvidia*\n"\
"\n"\
"    # Check for errors during removal\n"\
"    if [ $? -ne 0 ]; then\n"\
"        echo \"❌ Failed to remove NVIDIA drivers\"\n"\
"        yad --title=\"Removal Failed\" \\\n"\
"            --width=400 --height=200 \\\n"\
"            --image=\"dialog-error\" \\\n"\
"            --text=\"<b>❌ Failed to remove NVIDIA drivers.</b>\\n\\nPlease check your package manager logs for more details.\" \\\n"\
"            --button=\"OK\":0 \\\n"\
"            --center\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    echo \"✔️ NVIDIA drivers successfully removed\"\n"\
"\n"\
"    # Show a dialog confirming successful removal and offer to install drivers\n"\
"    yad --title=\"Removal Complete\" \\\n"\
"        --width=400 --height=200 \\\n"\
"        --image=\"dialog-information\" \\\n"\
"        --text=\"<b>✔️ NVIDIA drivers successfully removed.</b>\\n\\nInstall NVIDIA drivers 535 now.\" \\\n"\
"        --button=\"Install Drivers\":0 \\\n"\
"        --button=\"Close\":1 \\\n"\
"        --center\n"\
"\n"\
"    # Handle user decision\n"\
"    if [[ $? -eq 0 ]]; then\n"\
"        install_nvidia_drivers\n"\
"    else\n"\
"        echo \"Driver installation postponed by user.\"\n"\
"    fi\n"\
"}\n"\
"\n"\
"\n"\
"install_nvidia_drivers() {\n"\
"    # Show a temporary pop-up dialog for 3 seconds\n"\
"    (\n"\
"        yad --title=\"Installing NVIDIA Drivers\" \\\n"\
"            --width=400 --height=150 \\\n"\
"            --text-align=center \\\n"\
"            --no-buttons \\\n"\
"            --text=\"<b>Installing NVIDIA drivers, please wait...</b>\" \\\n"\
"            --timeout=3 \\\n"\
"            --timeout-indicator=top \\\n"\
"            --center\n"\
"    ) &\n"\
"\n"\
"    # Update package lists\n"\
"    echo \"===== Installing NVIDIA Driver 535 =====\"\n"\
"    echo \"Updating package lists...\"\n"\
"    sudo apt-get update\n"\
"\n"\
"    # Install specific NVIDIA driver version\n"\
"    echo \"Installing NVIDIA drivers version 535...\"\n"\
"    sudo apt-get install -y \\\n"\
"        nvidia-utils-535 \\\n"\
"        nvidia-driver-535\n"\
"\n"\
"    # Check installation status\n"\
"    if [ $? -ne 0 ]; then\n"\
"        echo \"❌ Failed to install NVIDIA drivers\"\n"\
"        yad --title=\"Installation Failed\" \\\n"\
"            --width=400 --height=200 \\\n"\
"            --image=\"dialog-error\" \\\n"\
"            --text=\"<b>❌ NVIDIA driver installation failed.</b>\\n\\nPlease check your package manager logs for more details.\" \\\n"\
"            --button=\"OK\":0 \\\n"\
"            --center\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Success message\n"\
"    echo \"✔️ NVIDIA drivers installed successfully. Please reboot the system to take effect.\"\n"\
"    yad --title=\"Installation Complete\" \\\n"\
"        --width=400 --height=200 \\\n"\
"        --image=\"dialog-information\" \\\n"\
"        --text=\"<b>✔️ NVIDIA drivers installed successfully.</b>\\n\\nPlease reboot the system to apply changes.\" \\\n"\
"        --button=\"Reboot Now\":0 \\\n"\
"        --button=\"Close\":1 \\\n"\
"        --center\n"\
"\n"\
"    # Handle reboot decision\n"\
"    if [[ $? -eq 0 ]]; then\n"\
"        echo \"Rebooting system...\"\n"\
"        sudo reboot now\n"\
"    else\n"\
"        echo \"Reboot postponed by user.\"\n"\
"    fi\n"\
"}\n"\
"\n"\
"\n"\
"install_cuda_toolkit() {\n"\
"    # Single dialog for progress, success, or failure\n"\
"    yad --title=\"Installing CUDA Toolkit\" \\\n"\
"        --width=450 --height=200 \\\n"\
"        --text=\"<b>🔄 Installing CUDA Toolkit (version 12.3), please wait...</b>\" \\\n"\
"        --text-align=center \\\n"\
"        --image=\"dialog-information\" \\\n"\
"        --no-buttons --center &\n"\
"    YAD_PID=$!  # Capture PID to close it later\n"\
"\n"\
"    echo \"===== Installing CUDA Toolkit 12.3 =====\"\n"\
"    \n"\
"    # Step 1: Download the CUDA keyring\n"\
"    echo \"Downloading CUDA keyring...\"\n"\
"    local CUDA_KEYRING=\"cuda-keyring_1.1-1_all.deb\"\n"\
"    local CUDA_REPO_URL=\"https://developer.download.nvidia.com/compute/cuda/repos/debian12/x86_64/$CUDA_KEYRING\"\n"\
"\n"\
"    if ! wget -q \"$CUDA_REPO_URL\" -O \"$CUDA_KEYRING\"; then\n"\
"        echo \"✘ Error: Failed to download CUDA keyring.\"\n"\
"        kill $YAD_PID 2>/dev/null  # Close the progress dialog\n"\
"        yad --title=\"Installation Failed\" \\\n"\
"            --width=450 --height=200 \\\n"\
"            --image=\"dialog-error\" \\\n"\
"            --text=\"<b>❌ CUDA Toolkit installation failed.</b>\\n\\n<b>Reason:</b> Could not download the CUDA keyring.\\n\\nPlease check your internet connection and try again.\" \\\n"\
"            --button=\"OK\":0 --center\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Step 2: Install the CUDA keyring\n"\
"    echo \"Installing CUDA keyring...\"\n"\
"    if ! sudo dpkg -i \"$CUDA_KEYRING\"; then\n"\
"        echo \"✘ Error: Failed to install CUDA keyring.\"\n"\
"        kill $YAD_PID 2>/dev/null\n"\
"        yad --title=\"Installation Failed\" \\\n"\
"            --width=450 --height=200 \\\n"\
"            --image=\"dialog-error\" \\\n"\
"            --text=\"<b>❌ CUDA Toolkit installation failed.</b>\\n\\n<b>Reason:</b> Could not install the CUDA keyring.\\n\\nPlease check your package manager settings or permissions.\" \\\n"\
"            --button=\"OK\":0 --center\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Step 3: Update the package list\n"\
"    echo \"Updating package list...\"\n"\
"    if ! sudo apt-get update; then\n"\
"        echo \"✘ Error: Failed to update package list.\"\n"\
"        kill $YAD_PID 2>/dev/null\n"\
"        yad --title=\"Installation Failed\" \\\n"\
"            --width=450 --height=200 \\\n"\
"            --image=\"dialog-error\" \\\n"\
"            --text=\"<b>❌ CUDA Toolkit installation failed.</b>\\n\\n<b>Reason:</b> Could not update the package list after adding the CUDA repository.\\n\\nPlease check your system's package manager.\" \\\n"\
"            --button=\"OK\":0 --center\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Step 4: Install CUDA Toolkit\n"\
"    echo \"Installing CUDA Toolkit version 12.3...\"\n"\
"    if ! sudo apt-get install -y cuda-toolkit-12-3; then\n"\
"        echo \"✘ Error: Failed to install CUDA Toolkit.\"\n"\
"        kill $YAD_PID 2>/dev/null\n"\
"        yad --title=\"Installation Failed\" \\\n"\
"            --width=450 --height=200 \\\n"\
"            --image=\"dialog-error\" \\\n"\
"            --text=\"<b>❌ CUDA Toolkit installation failed.</b>\\n\\n<b>Reason:</b> Could not install the CUDA Toolkit version 12.3.\\n\\nPlease check your package manager logs for more details.\" \\\n"\
"            --button=\"OK\":0 --center\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Step 5: Add CUDA to the system PATH\n"\
"    echo \"Adding CUDA to the system PATH...\"\n"\
"    if ! grep -q \"/usr/local/cuda/bin\" ~/.bashrc; then\n"\
"        echo 'export PATH=\"/usr/local/cuda/bin${PATH:+:${PATH}}\"' >> ~/.bashrc\n"\
"        source ~/.bashrc\n"\
"    fi\n"\
"\n"\
"    # Close the progress dialog\n"\
"    kill $YAD_PID 2>/dev/null\n"\
"\n"\
"    # Success message dialog\n"\
"    echo \"✔️ CUDA Toolkit version 12.3 installed successfully.\"\n"\
"    yad --title=\"Installation Complete\" \\\n"\
"        --width=450 --height=200 \\\n"\
"        --image=\"dialog-information\" \\\n"\
"        --text=\"<b>✔️ CUDA Toolkit version 12.3 installed successfully.</b>\\n\\n<b>Action:</b> Click 'Deploy Middleware'\" \\\n"\
"        --button=\"Deploy Middleware\":0 \\\n"\
"        --button=\"Close\":1 \\\n"\
"        --center\n"\
"\n"\
"    # Handle user decision\n"\
"    if [[ $? -eq 0 ]]; then\n"\
"        # Call the deploying aiDAPTIV function and check its success\n"\
"        if deploy_aiDAPTIV; then\n"\
"            echo \"\"\n"\
"        else\n"\
"            # If the function fails, show error dialog with yad\n"\
"            yad --title \"Error\" \\\n"\
"                --width=300 \\\n"\
"                --height=100 \\\n"\
"                --center \\\n"\
"                --button=Close:1 \\\n"\
"                --text=\"Error: Middleware deployment failed. Please check logs for details.\"\n"\
"            # Exit the script with an error status\n"\
"            exit 1\n"\
"        fi\n"\
"    fi\n"\
"}\n"\
"\n"\
"# Function to remove CUDA Toolkit\n"\
"remove_cuda_toolkit() {\n"\
"    yad --title=\"CUDA Removal\" --width=500 --height=200 \\\n"\
"        --text=\"🔄 Removing CUDA Toolkit. Please wait...\" --center --no-buttons &\n"\
"    sleep 2\n"\
"    kill $!\n"\
"\n"\
"    echo \"Removing CUDA Toolkit...\"\n"\
"    CUDA_PACKAGES=$(dpkg -l | grep 'cuda-toolkit-' | awk '{print $2}')\n"\
"    for package in $CUDA_PACKAGES; do\n"\
"        sudo apt-get remove -y \"$package\" || REMOVAL_SUCCESSFUL=false\n"\
"    done\n"\
"    sudo apt-get autoremove -y\n"\
"    sudo apt-get autoclean\n"\
"    sudo rm -rf /usr/local/cuda*\n"\
"    sed -i '/cuda/d' ~/.bashrc\n"\
"    source ~/.bashrc\n"\
"    \n"\
"    yad --title=\"CUDA Removal\" --width=500 --height=200 \\\n"\
"        --text=\"✔️ CUDA Toolkit removal completed successfully.\" --button=\"Install CUDA\":0 --center\n"\
"    if [[ $? -ne 0 ]]; then exit 1; fi\n"\
"    install_cuda_toolkit\n"\
"}\n"\
"\n"\
"\n"\
"check_nvidia_driver() {\n"\
"    # Helper function to print both to the terminal and GUI\n"\
"print_message() {\n"\
"    local title=$1\n"\
"    local message=$2\n"\
"    local icon=$3\n"\
"    local buttons=$4\n"\
"\n"\
"    # Remove HTML tags for terminal output\n"\
"    local terminal_message=$(echo \"$message\" | sed 's/<[^>]*>//g')\n"\
"\n"\
"    # Print the message to the terminal\n"\
"    echo -e \"\\n===== $title =====\\n$terminal_message\\n\"\n"\
"\n"\
"    # Display the message in the GUI\n"\
"    yad --title=\"$title\" \\\n"\
"        --width=500 --height=250 --text-align=center \\\n"\
"        --image=\"$icon\" \\\n"\
"        --text=\"$message\" \\\n"\
"        $buttons \\\n"\
"        --button=\"Close\":1 \\\n"\
"        --center\n"\
"}\n"\
"\n"\
"    # Check if an NVIDIA GPU is detected\n"\
"    if ! lshw -C display | grep -i nvidia &> /dev/null; then\n"\
"        print_message \"NVIDIA Driver Check - GPU Detection\" \\\n"\
"            \"<b>❌ No NVIDIA GPU detected in the system.</b>\\n\\n<b>Action Required:</b>\\nPlease ensure your system has an NVIDIA GPU installed to proceed.\" \\\n"\
"            \"dialog-error\" \n"\
"        exit 1\n"\
"    fi\n"\
"\n"\
"    # Check if the NVIDIA kernel module is loaded\n"\
"    if ! lsmod | grep -q nvidia; then\n"\
"        print_message \"NVIDIA Driver Check - Kernel Module\" \\\n"\
"            \"<b>❌ NVIDIA drivers are not installed or the kernel module is not loaded.</b>\\n\\n<b>Action Required:</b>\\nPlease install the NVIDIA drivers to proceed.\" \\\n"\
"            \"dialog-warning\" \\\n"\
"            \"--button=Install NVIDIA Driver:0\"\n"\
"        if [[ $? -ne 0 ]]; then exit 1; fi\n"\
"        install_nvidia_drivers\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Check if `nvidia-smi` is available to verify driver installation\n"\
"    if command -v nvidia-smi &> /dev/null; then\n"\
"        driver_version=$(nvidia-smi | grep \"Driver Version\" | awk '{print $3}')\n"\
"    else\n"\
"        driver_version=\"\"\n"\
"    fi\n"\
"    #driver_version=535.183.0\n"\
"    # Check if the driver version matches the required version\n"\
"    if [[ \"$driver_version\" == 535* ]]; then\n"\
"        print_message \"NVIDIA Driver Check - Driver Version\" \\\n"\
"            \"<b>✔️ Compatible NVIDIA driver version found:</b> <i>$driver_version</i>.\\n\\n<b>Action:</b>\\nClick 'Next' to proceed to CUDA checks.\" \\\n"\
"            \"dialog-information\" \\\n"\
"            \"--button=Next:0\"\n"\
"        if [[ $? -ne 0 ]]; then exit 1; fi\n"\
"        check_cuda\n"\
"    else\n"\
"        print_message \"NVIDIA Driver Check - Incompatible Driver\" \\\n"\
"            \"<b>⚠️ Existing NVIDIA driver version:</b> <i>$driver_version</i>.\\n\\n<b>Issue:</b>\\nThis is not compatible with the required version <b>535</b>.\\n\\n<b>Action Required:</b>\\nRemove the existing driver and install the correct version.\" \\\n"\
"            \"dialog-warning\" \\\n"\
"            \"--button=Remove Driver:0\"\n"\
"        if [[ $? -ne 0 ]]; then exit 1; fi\n"\
"        remove_nvidia_drivers\n"\
"    fi\n"\
"}\n"\
"\n"\
"# Function to check CUDA Toolkit version\n"\
"check_cuda() {\n"\
"    # Helper function to print to both terminal and GUI\n"\
"    print_message() {\n"\
"        local title=$1\n"\
"        local message=$2\n"\
"        local icon=$3\n"\
"        local buttons=$4\n"\
"\n"\
"        # Remove HTML tags for terminal output\n"\
"        local terminal_message=$(echo \"$message\" | sed 's/<[^>]*>//g')\n"\
"\n"\
"        # Print to terminal\n"\
"        echo -e \"\\n===== $title =====\\n$terminal_message\\n\"\n"\
"\n"\
"        # Show GUI dialog\n"\
"        yad --title=\"$title\" \\\n"\
"            --width=500 --height=250 --text-align=center \\\n"\
"            --image=\"$icon\" \\\n"\
"            --text=\"$message\" \\\n"\
"            $buttons \\\n"\
"            --center\n"\
"    }\n"\
"\n"\
"    # Check if CUDA Toolkit is installed\n"\
"    echo \"Checking for installed CUDA Toolkit...\"\n"\
"    local cuda_version=$(dpkg -l cuda-toolkit-[0-9]* 2>/dev/null | grep -E \"^ii\\s+cuda-toolkit-[0-9]+-[0-9]+\" | awk '{print $3}' | cut -d. -f1-2 | head -n 1)\n"\
"\n"\
"    if [ -z \"$cuda_version\" ]; then\n"\
"        print_message \"CUDA Check\" \\\n"\
"            \"<b>❌ CUDA Toolkit is NOT installed.</b>\\n\\n<b>Action Required:</b>\\nPlease install the CUDA Toolkit to proceed.\" \\\n"\
"            \"dialog-error\" \\\n"\
"            \"--button=Install CUDA:0 --button=Close:1\"\n"\
"        if [[ $? -ne 0 ]]; then\n"\
"            echo \"CUDA installation canceled by the user.\"\n"\
"            exit 1\n"\
"        fi\n"\
"        install_cuda_toolkit\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # CUDA version check\n"\
"    if [ \"$cuda_version\" == \"12.3\" ]; then\n"\
"        print_message \"CUDA Check\" \\\n"\
"            \"<b>✔️ CUDA Toolkit is installed.</b>\\n\\n<b>Version:</b> 12.3\\n\\n<b>Action:</b>\\nProceed with Deploying aiDAPTIV+ Middleware\" \\\n"\
"            \"dialog-information\" \\\n"\
"            \"--button=Deploy Middleware:0 --button=Close:1\"\n"\
"        if [[ $? -eq 0 ]]; then\n"\
"            if deploy_aiDAPTIV; then\n"\
"                echo \"\"\n"\
"            else\n"\
"                # If the function fails, show error dialog with yad\n"\
"                yad --title \"Error\" \\\n"\
"                    --width=300 \\\n"\
"                    --height=100 \\\n"\
"                    --center \\\n"\
"                    --button=Close:1 \\\n"\
"                    --text=\"Error: Middleware deployment failed. Please check logs for details.\"\n"\
"                # Exit the script with an error status\n"\
"                exit 1\n"\
"            fi\n"\
"        else\n"\
"            echo \"CUDA check completed. Middleware deployment skipped.\"\n"\
"        fi\n"\
"        return 0\n"\
"    else\n"\
"        print_message \"CUDA Check\" \\\n"\
"            \"<b>⚠️ CUDA Toolkit is installed, but the version is $cuda_version.</b>\\n\\n<b>Issue:</b>\\nVersion $cuda_version is not compatible. Recommended version is <b>12.3</b>.\\n\\n<b>Action Required:</b>\\nRemove the current version and install version 12.3.\" \\\n"\
"            \"dialog-warning\" \\\n"\
"            \"--button=Remove CUDA:0 --button=Cancel:1\"\n"\
"        if [[ $? -ne 0 ]]; then\n"\
"            echo \"CUDA removal canceled by the user.\"\n"\
"            exit 1\n"\
"        fi\n"\
"        remove_cuda_toolkit\n"\
"        return 2\n"\
"    fi\n"\
"}\n"\
"\n"\
"\n"\
"\n"\
"deploy_aiDAPTIV() {\n"\
"    # Progress dialog while deployment starts\n"\
"    (\n"\
"        yad --title=\"Middleware Deployment\" \\\n"\
"            --width=450 --height=200 \\\n"\
"            --text-align=center \\\n"\
"            --no-buttons \\\n"\
"            --image=\"dialog-information\" \\\n"\
"            --text=\"<b>🔄 Deploying aiDAPTIV middleware. Please wait...</b>\" \\\n"\
"            --timeout=5 \\\n"\
"            --timeout-indicator=top \\\n"\
"            --center\n"\
"    ) &\n"\
"\n"\
"    YAD_PID=$! # Capture the background dialog's process ID\n"\
"\n"\
"    echo \"===== Starting aiDAPTIV Middleware Deployment =====\"\n"\
"\n"\
"    # Update system packages\n"\
"    if ! sudo apt update; then\n"\
"        echo \"Failed to update system packages.\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Install required dependencies\n"\
"    if ! sudo apt install -y wget libaio1 libaio-dev liburing2 liburing-dev libboost-all-dev python3-pip libstdc++-12-dev; then\n"\
"        echo \"Failed to install required dependencies.\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Install GCC and G++ 12\n"\
"    if ! sudo apt install -y gcc-12 g++-12; then\n"\
"        echo \"Failed to install GCC and G++ 12.\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Set up alternative for G++\n"\
"    if ! sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 50; then\n"\
"        echo \"Failed to set up alternative for G++.\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Check pip path\n"\
"    PYTHON_PATH=$(which python3 | xargs dirname)\n"\
"    if [ \"$PYTHON_PATH\" == \"\" ]; then\n"\
"        echo \"Python3 Path is not found. Please check 'which python3' working correctly.\"\n"\
"        return 1\n"\
"    fi\n"\
"    echo \"Found python3 at:\" $PYTHON_PATH\n"\
"\n"\
"    # Get current user\n"\
"    current_user=$(whoami)\n"\
"\n"\
"    # Create directories\n"\
"    if ! sudo mkdir -p /home/$current_user/; then\n"\
"        echo \"Failed to create directory /home/$current_user/.\"\n"\
"        return 1\n"\
"    fi\n"\
"    if ! sudo mkdir -p /home/$current_user/Desktop/; then\n"\
"        echo \"Failed to create directory /home/$current_user/Desktop/.\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Delete old aiDAPTIV+ directories\n"\
"    if ! sudo rm -rf /home/$current_user/Desktop/dm /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"        echo \"Failed to delete old aiDAPTIV+ directories.\"\n"\
"        return 1\n"\
"    fi\n"\
"    if ! sudo mkdir /home/$current_user/Desktop/dm /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"        echo \"Failed to create new aiDAPTIV+ directories.\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Download aiDAPTIV+ package\n"\
"    TAR_NAME=\"vNXUN_2_01_00.tar\"\n"\
"    if ! sudo rm -f $TAR_NAME; then\n"\
"        echo \"Failed to remove old $TAR_NAME file.\"\n"\
"        return 1\n"\
"    fi\n"\
"    if ! wget --tries=3 https://phisonbucket.s3.ap-northeast-1.amazonaws.com/$TAR_NAME --no-check-certificate; then\n"\
"        read -p \"Can't get $TAR_NAME from cloud, Please enter the path to the $TAR_NAME file: \" filepath\n"\
"        if ! sudo tar xvf \"$filepath\" -C /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"            echo \"Failed to extract $TAR_NAME from provided path.\"\n"\
"            return 1\n"\
"        fi\n"\
"    else\n"\
"        if ! sudo tar xvf $TAR_NAME -C /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"            echo \"Failed to extract $TAR_NAME from cloud.\"\n"\
"            return 1\n"\
"        fi\n"\
"    fi\n"\
"    echo 'unzip package'\n"\
"\n"\
"    # Build environment\n"\
"    echo \"Start to build env...\"\n"\
"\n"\
"    # Install required packages using pip\n"\
"    if [[ \"$PYTHON_PATH\" == \"/usr/bin\" && \"$current_user\" != \"root\" ]]; then\n"\
"        if ! yes | pip install --user -r /home/$current_user/Desktop/aiDAPTIV2/requirements.txt; then\n"\
"            echo \"Failed to install required packages in user space.\"\n"\
"            return 1\n"\
"        fi\n"\
"        appenvpath /home/$current_user/.local/bin\n"\
"    else\n"\
"        if ! yes | pip install -r /home/$current_user/Desktop/aiDAPTIV2/requirements.txt; then\n"\
"            echo \"Failed to install required packages.\"\n"\
"            return 1\n"\
"        fi\n"\
"        if [ \"$current_user\" == \"root\" ]; then\n"\
"            echo \"installed required packages in /usr/local/lib/python3.10/dist-packages\"\n"\
"        else\n"\
"            echo \"installed required packages in\"  $(realpath $PYTHON_PATH/../lib/python3.10/site-packages/)\n"\
"        fi\n"\
"    fi\n"\
"\n"\
"    # Change directory to aiDAPTIV2\n"\
"    if ! cd /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Set executable permissions\n"\
"    if ! sudo chmod +x bin/*; then\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Move and set permissions for phisonlib\n"\
"    if ! sudo mv *.so ./phisonlib; then\n"\
"        return 1\n"\
"    fi\n"\
"    if ! sudo chmod +x ./phisonlib/ada.exe; then\n"\
"        return 1\n"\
"    fi\n"\
"    if ! sudo setcap cap_sys_admin,cap_dac_override=+eip ./phisonlib/ada.exe; then\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Move bin files to dm directory and user-local bin directory if necessary\n"\
"    if [[ \"$PYTHON_PATH\" == \"/usr/bin\" && \"$current_user\" != \"root\" ]]; then\n"\
"        if ! sudo cp bin/* /home/$current_user/Desktop/dm/; then\n"\
"            return 1\n"\
"        fi\n"\
"        if ! sudo mv bin/* /home/$current_user/.local/bin/; then\n"\
"            return 1\n"\
"        fi\n"\
"        if ! sudo rm -rf bin; then\n"\
"            return 1\n"\
"        fi\n"\
"        echo 'Moved bin files'\n"\
"        \n"\
"        if ! sudo rm -rf /home/$current_user/.local/lib/python3.10/site-packages/phisonlib; then\n"\
"            return 1\n"\
"        fi\n"\
"        if ! sudo mv phisonlib /home/$current_user/.local/lib/python3.10/site-packages/; then\n"\
"            return 1\n"\
"        fi\n"\
"        if ! sudo rm -rf phisonlib; then\n"\
"            return 1\n"\
"        fi\n"\
"    else\n"\
"        if ! sudo cp bin/* /home/$current_user/Desktop/dm/; then\n"\
"            return 1\n"\
"        fi\n"\
"        if ! sudo mv bin/* $PYTHON_PATH; then\n"\
"            return 1\n"\
"        fi\n"\
"        if ! sudo rm -rf bin; then\n"\
"            return 1\n"\
"        fi\n"\
"        echo 'Move bin files to dm/ and ' $PYTHON_PATH\n"\
"        \n"\
"        if [ \"$current_user\" == \"root\" ]; then\n"\
"            if ! sudo rm -rf /usr/local/lib/python3.10/dist-packages/phisonlib; then\n"\
"                return 1\n"\
"            fi\n"\
"            if ! sudo mv phisonlib /usr/local/lib/python3.10/dist-packages; then\n"\
"                return 1\n"\
"            fi\n"\
"            if ! sudo rm -rf phisonlib; then\n"\
"                return 1\n"\
"            fi\n"\
"        else\n"\
"            if ! sudo rm -rf  $PYTHON_PATH/../lib/python3.10/site-packages/phisonlib; then\n"\
"                return 1\n"\
"            fi\n"\
"            if ! sudo mv phisonlib $PYTHON_PATH/../lib/python3.10/site-packages/; then\n"\
"                return 1\n"\
"            fi\n"\
"            if ! sudo rm -rf phisonlib; then\n"\
"                return 1\n"\
"            fi\n"\
"        fi\n"\
"    fi\n"\
"\n"\
"    # echo 'export PYTHONPATH=\"/home/$USER/.local/lib/python3.10/site-packages/Phisonlib\"' >> ~/.bashrc\n"\
"    # echo 'export PYTHONPATH=\"/home/$USER/Desktop/aiDAPTIV2:$PYTHONPATH\"' >> ~/.bashrc\n"\
"    # Close progress dialog\n"\
"    kill $YAD_PID 2>/dev/null\n"\
"\n"\
"    # Step 8: Final success message\n"\
"    echo \"✔️ aiDAPTIV middleware deployed successfully.\"\n"\
"    yad --title=\"Deployment Complete\" \\\n"\
"        --width=450 --height=200 \\\n"\
"        --image=\"dialog-information\" \\\n"\
"        --text=\"<b>✔️ Middleware deployed successfully.</b>\\n\" \\\n"\
"        --button=\"OK\":0 --center\n"\
"}\n"\
"\n"\
"\n"\
"\n"\
"deploy_middleware() {\n"\
"    local output=\"\"\n"\
"    local failed_output=\"\"\n"\
"    local success=true\n"\
"\n"\
"    # Function to add check results to the output\n"\
"    add_to_output() {\n"\
"        local title=$1\n"\
"        local result=$2\n"\
"        local status=$3\n"\
"        output+=\"$title:\\n$result\\n\\n\"\n"\
"        if [[ $status -ne 0 ]]; then\n"\
"            success=false\n"\
"            failed_output+=\"$title:\\n$result\\n\\n\"  # Add the failed check's result to the failed_output\n"\
"        fi\n"\
"    }\n"\
"\n"\
"    # Disk Space Check\n"\
"    space_result=$(check_space \"20G\")\n"\
"    add_to_output \"Disk Space Check\" \"$space_result\" $?\n"\
"\n"\
"    # Python3 Check\n"\
"    python_result=$(check_python3)\n"\
"    add_to_output \"Python3 Check\" \"$python_result\" $?\n"\
"\n"\
"    # Package Manager Check\n"\
"    package_result=$(check_package_manager)\n"\
"    add_to_output \"Package Manager Check\" \"$package_result\" $?\n"\
"\n"\
"    # Output all results to the terminal\n"\
"    echo -e \"\\n===== System Validation Results =====\"\n"\
"    echo -e \"$output\"\n"\
"\n"\
"    # Prepare the dialog message\n"\
"    local dialog_message=\"\"\n"\
"    if [[ $success == true ]]; then\n"\
"        dialog_message=\"✅ All system checks passed successfully!\"\n"\
"    else\n"\
"        dialog_message=\"❌ System validation failed:\\n\\n$failed_output\"\n"\
"    fi\n"\
"\n"\
"    # Display results in a dialog\n"\
"    if [[ $success == true ]]; then\n"\
"        yad --title=\"System Validation\" \\\n"\
"            --width=500 --height=300 \\\n"\
"            --text-align=center \\\n"\
"            --text=\"<b>$dialog_message</b>\" \\\n"\
"            --button=\"Next\":0 \\\n"\
"            --button=\"Close\":1 \\\n"\
"            --center\n"\
"\n"\
"        case $? in\n"\
"            0) # Next button\n"\
"                return 0\n"\
"                ;;\n"\
"            *) # Cancel button or window close\n"\
"                echo \"System validation cancelled by user.\"\n"\
"                exit 1\n"\
"                ;;\n"\
"        esac\n"\
"    else\n"\
"        yad --title=\"System Validation\" \\\n"\
"            --width=600 --height=400 \\\n"\
"            --text-align=left \\\n"\
"            --text=\"<b>$dialog_message</b>\" \\\n"\
"            --button=\"Close\":1 \\\n"\
"            --center\n"\
"\n"\
"        echo -e \"\\nSystem validation failed. Exiting.\"\n"\
"        exit 1\n"\
"    fi\n"\
"}\n"\
"\n"\
"\n"\
"# Main script execution\n"\
"deploy_middleware\n"\
"if [[ $? -eq 0 ]]; then\n"\
"    # Proceed to Nvidia driver check only if the user clicked \"Next\"\n"\
"    check_nvidia_driver\n"\
"fi\n"\
"\n"\
"echo \"===== Script Execution Complete =====\"\n"\
"",
        NULL
    };

    execve("/bin/bash", args, environ);

    perror("execve");
    return 1;
}

