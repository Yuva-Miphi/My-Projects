
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
"# Try creating or appending to the log file\n"\
"if ! exec > >(tee -a \"$LOG_FILE\") 2>&1; then\n"\
"    echo \"Permission denied: Unable to create or write to the log file $LOG_FILE. Please check the permissions.\"\n"\
"    exit 1\n"\
"fi\n"\
"\n"\
"\n"\
"current_dir=$(pwd)\n"\
"\n"\
"# To cleanup installation cache\n"\
"cleanup(){\n"\
"    # Purge packages in problematic states\n"\
"    dpkg -l | grep -E '^rc|^iU|^iF|^hF' | awk '{print $2}' | xargs -r sudo dpkg --purge >/dev/null 2>&1\n"\
"    sudo dpkg -l | grep -E '^(rc|iU|iF|hF)' | awk '{print $2}' | xargs -r sudo dpkg --remove --force-remove-reinstreq >/dev/null 2>&1\n"\
"    # Cleaning up from /var/cache/apt/archives\n"\
"    sudo apt-get clean\n"\
"    # Handle pip cleanup if pip is installed\n"\
"    if command -v pip &>/dev/null 2>&1; then\n"\
"        pip cache purge &>/dev/null 2>&1\n"\
"    fi\n"\
"    echo \"Exiting.....\"\n"\
"}\n"\
"\n"\
"#Trap EXIT signals\n"\
"trap cleanup EXIT\n"\
"\n"\
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
"remove_cuda_toolkit() {\n"\
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
"}\n"\
"\n"\
"\n"\
"# List of APT packages to check\n"\
"apt_packages=(\n"\
"    \"cuda-toolkit-12-3\"\n"\
"    \"g++-12\" \"gcc-12\"\n"\
"    \"libaio1\"\n"\
"    \"libaio-dev\"\n"\
"    \"libboost-all-dev\"\n"\
"    \"libstdc++-12-dev\"\n"\
"    \"liburing2\"\n"\
"    \"liburing-dev\"\n"\
"    \"lvm2\"\n"\
"    \"mdadm\"\n"\
"    \"nvidia-driver-535\"\n"\
"    \"python3-pip\"\n"\
"    \"vim\"\n"\
"    \"wget\"\n"\
")\n"\
"\n"\
"\n"\
"# Function to check if required APT packages are installed\n"\
"check_apt_packages() {\n"\
"    for package in \"${apt_packages[@]}\"; do\n"\
"        if ! sudo dpkg -l \"$package\" &>/dev/null; then\n"\
"            return 1\n"\
"        fi\n"\
"    done\n"\
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
"    yad --title=\"Removal Complete\" \\\n"\
"        --width=400 --height=200 \\\n"\
"        --image=\"dialog-information\" \\\n"\
"        --text=\"<b>✔️ NVIDIA drivers successfully removed.</b>\\n\\nInstall NVIDIA drivers and other packages now.\" \\\n"\
"        --button=\"Install\":0 \\\n"\
"        --button=\"Close\":1 \\\n"\
"        --center\n"\
"\n"\
"    # Handle user decision\n"\
"    if [[ $? -eq 0 ]]; then\n"\
"        install_deb_packages\n"\
"    fi\n"\
"}\n"\
"\n"\
"\n"\
"# Function to install APT packages\n"\
"install_deb_packages() {\n"\
"    # Path where all .deb files are available\n"\
"    local deb_dir=\"required_packages/apt_packages/all_debs\"\n"\
"    #Storing the current directory location.\n"\
"    local current_dir=$(pwd)\n"\
"\n"\
"    echo -e \"********** Starting Driver and other package Installation **********\"\n"\
"    if [ ! -d \"$deb_dir\" ]; then\n"\
"        echo \"✘ Error: Directory '$deb_dir' not found...\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    cd \"$deb_dir\" || { echo \"Error: Failed to enter directory $deb_dir\"; return 1; }\n"\
"\n"\
"    #install all deb files forcefully. (to override existing packages)\n"\
"    if ! sudo dpkg -i --force-all --auto-deconfigure *.deb; then\n"\
"        echo \"✘ Error: Installation of APT packages failed.. Try to Deploy again...\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    echo \"✓ ********** APT packages installation completed successfully **********\"\n"\
"    cd \"$current_dir\"\n"\
"\n"\
"        # Success message\n"\
"    echo \"Please reboot the system to take effect.\"\n"\
"    yad --title=\"Installation Complete\" \\\n"\
"        --width=400 --height=200 \\\n"\
"        --image=\"dialog-information\" \\\n"\
"        --text=\"<b>✔️ Installation successfull.</b>\\n\\nPlease reboot the system to apply changes.\" \\\n"\
"        --button=\"Reboot Now\":0 \\\n"\
"        --button=\"Close\":1 \\\n"\
"        --center\n"\
"\n"\
"    # Handle reboot decision\n"\
"    if [[ $? -eq 0 ]]; then\n"\
"        echo \"Rebooting system...\"\n"\
"        sudo reboot now\n"\
"    fi\n"\
"}\n"\
"\n"\
"\n"\
"check_driver() {\n"\
"    # Helper function to print both to the terminal and GUI\n"\
"    print_message() {\n"\
"        local title=$1\n"\
"        local message=$2\n"\
"        local icon=$3\n"\
"        local buttons=$4\n"\
"\n"\
"        # Remove HTML tags for terminal output\n"\
"        local terminal_message=$(echo \"$message\" | sed 's/<[^>]*>//g')\n"\
"\n"\
"        # Print the message to the terminal\n"\
"        echo -e \"\\n===== $title =====\\n$terminal_message\\n\"\n"\
"\n"\
"        # Display the message in the GUI\n"\
"        yad --title=\"$title\" \\\n"\
"            --width=500 --height=250 --text-align=center \\\n"\
"            --image=\"$icon\" \\\n"\
"            --text=\"$message\" \\\n"\
"            $buttons \\\n"\
"            --button=\"Close\":1 \\\n"\
"            --center\n"\
"    }\n"\
"\n"\
"    # Check if an NVIDIA GPU is detected\n"\
"    if ! lshw -C display | grep -i nvidia &> /dev/null; then\n"\
"        print_message \"NVIDIA Driver Check - GPU Detection\" \\\n"\
"            \"<b>❌ No NVIDIA GPU detected in the system.</b>\\n\\n<b>Action Required:</b>\\nPlease ensure your system has an NVIDIA GPU installed to proceed.\" \\\n"\
"            \"dialog-error\" \n"\
"        exit 1\n"\
"    fi\n"\
"\n"\
"    if ! lsmod | grep -q nvidia; then\n"\
"        print_message \"NVIDIA Driver Check\" \\\n"\
"            \"<b>❌ NVIDIA drivers are not installed.</b>\\n\\n<b>Action Required:</b>\\nPlease install the NVIDIA drivers and other required packages.\" \\\n"\
"            \"dialog-warning\" \\\n"\
"            \"--button=Install NVIDIA Driver:0\"\n"\
"        if [[ $? -ne 0 ]]; then exit 1; fi\n"\
"        install_deb_packages\n"\
"        return 1 \n"\
"    fi\n"\
"\n"\
"\n"\
"    if command -v nvidia-smi &> /dev/null; then\n"\
"        driver_version=$(nvidia-smi | grep \"Driver Version\" | awk '{print $3}')\n"\
"    else\n"\
"        driver_version=\"\"\n"\
"    fi\n"\
"\n"\
"    #after reboot\n"\
"    if [[ \"$driver_version\" == 535* ]]; check_apt_packages; then\n"\
"        echo \"Required Drivers (535.183) and other packages are found.. Proceed with deploying middleware...\"\n"\
"        print_message \"Drivers Found\" \\\n"\
"            \"<b>✔️ Required Drivers and other packages are found.</b>\\n<b>Action:</b>\\nProceed with Deploying aiDAPTIV+ Middleware\" \\\n"\
"            \"dialog-information\" \\\n"\
"            \"--button=Deploy Middleware:0\"\n"\
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
"        fi\n"\
"    else\n"\
"        print_message \"NVIDIA Driver Check - Incompatible Driver\" \\\n"\
"        \"<b>⚠️ Existing NVIDIA driver found or some required packages are missing.</b>\\n <b>Action Required:</b>\\nRemove the existing driver and install again.\" \\\n"\
"        \"dialog-warning\" \\\n"\
"        \"--button=Remove Driver:0\"\n"\
"        if [[ $? -ne 0 ]]; then exit 1; fi\n"\
"        remove_nvidia_drivers   \n"\
"    fi\n"\
"\n"\
"\n"\
"\n"\
"}\n"\
"\n"\
"\n"\
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
"\n"\
"# Updated function to install Python packages\n"\
"install_python_packages() {\n"\
"    PYTHON_PATH=$(which python3 | xargs dirname)\n"\
"    echo \"Found python3 at:\"$PYTHON_PATH\n"\
"\n"\
"    if [ \"$PYTHON_PATH\" == \"\" ]; then\n"\
"        echo \"✘ Python3 Path is not found. Please check 'which python3' working correctly.\"\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    current_user=$(whoami)\n"\
"    echo -e \"********** Starting Python Package Installation **********\"\n"\
"    #function to check if pip is installed\n"\
"    local current_dir=$(pwd)\n"\
"\n"\
"    if [[ \"$PYTHON_PATH\" == \"/usr/bin\" && \"$current_user\" != \"root\" ]]; then\n"\
"        local base_dir=\"required_packages/python_packages/all_wheels\"\n"\
"        echo \"**********Starting Python package installation (user-level)**********\"\n"\
"        cd \"$base_dir\" || { echo \"Failed to enter base directory $base_dir\"; return 1; }\n"\
"        if ! sudo -u $current_user python3 -m pip install --user --no-index --force-reinstall --find-links=. *.whl; then\n"\
"            echo \"✘ Error: Installation of Python packages failed.\"\n"\
"            return 1\n"\
"        fi\n"\
"        appenvpath /home/$current_user/.local/bin\n"\
"        echo \"✓ User-level installation completed successfully.\"\n"\
"\n"\
"    else\n"\
"        if [ \"$current_user\" == \"root\" ]; then\n"\
"            echo \"Installing required packages in /usr/local/lib/python3.10/dist-packages\"\n"\
"        else\n"\
"            echo \"Installing required packages in $(realpath $PYTHON_PATH/../lib/python3.10/site-packages/)\"\n"\
"        fi\n"\
"\n"\
"        local base_dir=\"required_packages/python_packages/all_wheels\"\n"\
"        echo \"**********Starting Python package installation (system-wide)**********\"\n"\
"        cd \"$base_dir\" || { echo \"Failed to enter base directory $base_dir\"; return 1; }\n"\
"\n"\
"        if ! pip install --no-index --force-reinstall --find-links=. *.whl; then\n"\
"            echo \"✘ Error: Installation of Python packages failed.\"\n"\
"            return 1\n"\
"        fi\n"\
"        echo \"✓ System-wide installation completed successfully.\"\n"\
"    fi\n"\
"    cd \"$current_dir\"\n"\
"}\n"\
"\n"\
"\n"\
"# Function to set up the aiDAPTIV2 package\n"\
"setup_aidaptiv_package()\n"\
"{\n"\
"    cd $current_dir\n"\
"    # Get current user\n"\
"    current_user=$(whoami)\n"\
"    # Create directories\n"\
"    if ! sudo mkdir -p /home/$current_user/; then\n"\
"        echo \"Failed to create directory /home/$current_user/.\"\n"\
"        return 1\n"\
"    fi\n"\
"    if ! sudo mkdir -p /home/$current_user/Desktop/; then\n"\
"        echo \"Failed to create directory /home/$current_user/Desktop/.\"\n"\
"        return 1\n"\
"    fi\n"\
"    # Delete old aiDAPTIV+ directories\n"\
"    if ! sudo rm -rf /home/$current_user/Desktop/dm /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"        echo \"Failed to delete old aiDAPTIV+ directories.\"\n"\
"        return 1\n"\
"    fi\n"\
"    if ! sudo mkdir /home/$current_user/Desktop/dm /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"        echo \"Failed to create new aiDAPTIV+ directories.\"\n"\
"        return 1\n"\
"    fi\n"\
"    # Download aiDAPTIV+ package\n"\
"    TAR_NAME=\"middleware/vNXUN_2_01_00.tar\"\n"\
"    if ! sudo tar xvf $TAR_NAME -C /home/$current_user/Desktop/aiDAPTIV2; then\n"\
"        echo \"Failed to extract $TAR_NAME.\"\n"\
"        return 1\n"\
"    fi\n"\
"    echo 'unzip package'    \n"\
"}\n"\
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
"            --timeout=2 \\\n"\
"            --timeout-indicator=top \\\n"\
"            --center\n"\
"    ) &\n"\
"\n"\
"    YAD_PID=$! # Capture the background dialog's process ID\n"\
"\n"\
"    echo \"===== Starting aiDAPTIV Middleware Deployment =====\"\n"\
"\n"\
"    if ! setup_aidaptiv_package; then \n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    if ! install_python_packages; then\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
"    # Get current user\n"\
"    current_user=$(whoami)\n"\
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
"\n"\
"    if ! sudo chmod +x ./phisonlib/ada.exe; then\n"\
"        return 1\n"\
"    fi\n"\
"\n"\
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
"    check_driver\n"\
"fi\n"\
"\n"\
"echo \"===== Script Execution Complete =====\"\n"\
"\n"\
"",
        NULL
    };

    execve("/bin/bash", args, environ);

    perror("execve");
    return 1;
}
