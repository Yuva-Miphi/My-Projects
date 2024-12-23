#!/bin/bash

# Start logging
echo "===== Starting System Validation ====="
echo "Timestamp: $(date)"
echo "======================================"

# Log file
LOG_FILE="aiDAPTIV_Middleware_Deployment.log"

# Redirect stdout and stderr to the log file
if ! exec > >(tee -a "$LOG_FILE") 2>&1; then
    echo "Permission denied: Unable to create or write to the log file $LOG_FILE. Please check the file permissions."
    exit 1
fi

# Append env paths
appenvpath () {

    case ":$PATH:" in
        *:"$1":*)
            echo 'ENV path exists'
            ;;
        *)
            echo 'export PATH="/home/$USER/.local/bin:$PATH"' >> ~/.bashrc
            echo 'added ENV path to ~/.bashrc'
    esac
}

# Function to check available disk space
check_space() {
    local required_space=$1
    required_bytes=$(numfmt --from=iec $required_space)
    available_space=$(df -h / | awk 'NR==2 {print $4}')
    available_bytes=$(df --block-size=1 / | awk 'NR==2 {print $4}')
    if [[ $available_bytes -ge $required_bytes ]]; then
        echo "✔ Sufficient space available. Required: $required_space, Available: $available_space"
        return 0
    else
        echo "✘ Insufficient space. Required: $required_space, Available: $available_space"
        return 1
    fi
}

# Function to check Python3 installation
check_python3() {
    PYTHON_PATH=$(which python3 | xargs dirname)
    if [ -z "$PYTHON_PATH" ]; then
        echo "✘ Python3 is not installed or not found in the PATH."
        return 1
    else
        echo "✔ Python3 is installed at: $PYTHON_PATH"
        return 0
    fi
}

# Function to check the package manager
check_package_manager() {
    if sudo apt-get update &> /dev/null; then
        echo "✔ Package manager (apt) is functioning correctly."
        return 0
    else
        echo "✘ Unable to update package lists. Problem with apt-get."
        return 1
    fi
}


remove_nvidia_drivers() {
    # Show a temporary pop-up dialog for 3 seconds
    (
        yad --title="Removing NVIDIA Drivers" \
            --width=400 --height=150 \
            --text-align=center \
            --no-buttons \
            --text="<b>Removing NVIDIA drivers, please wait...</b>" \
            --timeout=3 \
            --timeout-indicator=top \
            --center
    ) &

    echo "🔄 Starting NVIDIA driver removal process..."
    
    # Remove NVIDIA packages
    echo "Removing NVIDIA packages..."
    sudo apt-get remove -y --purge '^nvidia-.*'

    # Remove CUDA and related libraries
    echo "Removing CUDA and related libraries..."
    CUDA_PACKAGES=$(dpkg -l | grep 'cuda-toolkit-' | awk '{print $2}')
    for package in $CUDA_PACKAGES; do
        sudo apt-get remove -y "$package"
    done
    
    # Cleanup
    echo "Cleaning up residual packages..."
    sudo apt-get remove -y --purge cuda* libnvidia* nvidia-settings nvidia-prime
    sudo apt-get autoremove -y
    sudo apt-get autoclean -y
    sudo rm -rf /usr/local/cuda*
    sudo rm -rf /usr/share/nvidia*
    sed -i '/cuda/d' ~/.bashrc
    source ~/.bashrc

    # Final cleanup
    echo "Removing remaining NVIDIA configuration files..."
    sudo rm -rf /usr/local/cuda*
    sudo rm -rf /usr/share/nvidia*

    # Check for errors during removal
    if [ $? -ne 0 ]; then
        echo "❌ Failed to remove NVIDIA drivers"
        yad --title="Removal Failed" \
            --width=400 --height=200 \
            --image="dialog-error" \
            --text="<b>❌ Failed to remove NVIDIA drivers.</b>\n\nPlease check your package manager logs for more details." \
            --button="OK":0 \
            --center
        return 1
    fi

    echo "✔️ NVIDIA drivers successfully removed"

    # Show a dialog confirming successful removal and offer to install drivers
    yad --title="Removal Complete" \
        --width=400 --height=200 \
        --image="dialog-information" \
        --text="<b>✔️ NVIDIA drivers successfully removed.</b>\n\nInstall NVIDIA drivers 535 now." \
        --button="Install Drivers":0 \
        --button="Close":1 \
        --center

    # Handle user decision
    if [[ $? -eq 0 ]]; then
        install_nvidia_drivers
    else
        echo "Driver installation postponed by user."
    fi
}


install_nvidia_drivers() {
    # Show a temporary pop-up dialog for 3 seconds
    (
        yad --title="Installing NVIDIA Drivers" \
            --width=400 --height=150 \
            --text-align=center \
            --no-buttons \
            --text="<b>Installing NVIDIA drivers, please wait...</b>" \
            --timeout=3 \
            --timeout-indicator=top \
            --center
    ) &

    # Update package lists
    echo "===== Installing NVIDIA Driver 535 ====="
    echo "Updating package lists..."
    sudo apt-get update

    # Install specific NVIDIA driver version
    echo "Installing NVIDIA drivers version 535..."
    sudo apt-get install -y \
        nvidia-utils-535 \
        nvidia-driver-535

    # Check installation status
    if [ $? -ne 0 ]; then
        echo "❌ Failed to install NVIDIA drivers"
        yad --title="Installation Failed" \
            --width=400 --height=200 \
            --image="dialog-error" \
            --text="<b>❌ NVIDIA driver installation failed.</b>\n\nPlease check your package manager logs for more details." \
            --button="OK":0 \
            --center
        return 1
    fi

    # Success message
    echo "✔️ NVIDIA drivers installed successfully. Please reboot the system to take effect."
    yad --title="Installation Complete" \
        --width=400 --height=200 \
        --image="dialog-information" \
        --text="<b>✔️ NVIDIA drivers installed successfully.</b>\n\nPlease reboot the system to apply changes." \
        --button="Reboot Now":0 \
        --button="Close":1 \
        --center

    # Handle reboot decision
    if [[ $? -eq 0 ]]; then
        echo "Rebooting system..."
        sudo reboot now
    else
        echo "Reboot postponed by user."
    fi
}


install_cuda_toolkit() {
    # Single dialog for progress, success, or failure
    yad --title="Installing CUDA Toolkit" \
        --width=450 --height=200 \
        --text="<b>🔄 Installing CUDA Toolkit (version 12.3), please wait...</b>" \
        --text-align=center \
        --image="dialog-information" \
        --no-buttons --center &
    YAD_PID=$!  # Capture PID to close it later

    echo "===== Installing CUDA Toolkit 12.3 ====="
    
    # Step 1: Download the CUDA keyring
    echo "Downloading CUDA keyring..."
    local CUDA_KEYRING="cuda-keyring_1.1-1_all.deb"
    local CUDA_REPO_URL="https://developer.download.nvidia.com/compute/cuda/repos/debian12/x86_64/$CUDA_KEYRING"

    if ! wget -q "$CUDA_REPO_URL" -O "$CUDA_KEYRING"; then
        echo "✘ Error: Failed to download CUDA keyring."
        kill $YAD_PID 2>/dev/null  # Close the progress dialog
        yad --title="Installation Failed" \
            --width=450 --height=200 \
            --image="dialog-error" \
            --text="<b>❌ CUDA Toolkit installation failed.</b>\n\n<b>Reason:</b> Could not download the CUDA keyring.\n\nPlease check your internet connection and try again." \
            --button="OK":0 --center
        return 1
    fi

    # Step 2: Install the CUDA keyring
    echo "Installing CUDA keyring..."
    if ! sudo dpkg -i "$CUDA_KEYRING"; then
        echo "✘ Error: Failed to install CUDA keyring."
        kill $YAD_PID 2>/dev/null
        yad --title="Installation Failed" \
            --width=450 --height=200 \
            --image="dialog-error" \
            --text="<b>❌ CUDA Toolkit installation failed.</b>\n\n<b>Reason:</b> Could not install the CUDA keyring.\n\nPlease check your package manager settings or permissions." \
            --button="OK":0 --center
        return 1
    fi

    # Step 3: Update the package list
    echo "Updating package list..."
    if ! sudo apt-get update; then
        echo "✘ Error: Failed to update package list."
        kill $YAD_PID 2>/dev/null
        yad --title="Installation Failed" \
            --width=450 --height=200 \
            --image="dialog-error" \
            --text="<b>❌ CUDA Toolkit installation failed.</b>\n\n<b>Reason:</b> Could not update the package list after adding the CUDA repository.\n\nPlease check your system's package manager." \
            --button="OK":0 --center
        return 1
    fi

    # Step 4: Install CUDA Toolkit
    echo "Installing CUDA Toolkit version 12.3..."
    if ! sudo apt-get install -y cuda-toolkit-12-3; then
        echo "✘ Error: Failed to install CUDA Toolkit."
        kill $YAD_PID 2>/dev/null
        yad --title="Installation Failed" \
            --width=450 --height=200 \
            --image="dialog-error" \
            --text="<b>❌ CUDA Toolkit installation failed.</b>\n\n<b>Reason:</b> Could not install the CUDA Toolkit version 12.3.\n\nPlease check your package manager logs for more details." \
            --button="OK":0 --center
        return 1
    fi

    # Step 5: Add CUDA to the system PATH
    echo "Adding CUDA to the system PATH..."
    if ! grep -q "/usr/local/cuda/bin" ~/.bashrc; then
        echo 'export PATH="/usr/local/cuda/bin${PATH:+:${PATH}}"' >> ~/.bashrc
        source ~/.bashrc
    fi

    # Close the progress dialog
    kill $YAD_PID 2>/dev/null

    # Success message dialog
    echo "✔️ CUDA Toolkit version 12.3 installed successfully."
    yad --title="Installation Complete" \
        --width=450 --height=200 \
        --image="dialog-information" \
        --text="<b>✔️ CUDA Toolkit version 12.3 installed successfully.</b>\n\n<b>Action:</b> Click 'Deploy Middleware'" \
        --button="Deploy Middleware":0 \
        --button="Close":1 \
        --center

    # Handle user decision
    if [[ $? -eq 0 ]]; then
        # Call the deploying aiDAPTIV function and check its success
        if deploy_aiDAPTIV; then
            echo ""
        else
            # If the function fails, show error dialog with yad
            yad --title "Error" \
                --width=300 \
                --height=100 \
                --center \
                --button=Close:1 \
                --text="Error: Middleware deployment failed. Please check logs for details."
            # Exit the script with an error status
            exit 1
        fi
    fi
}

# Function to remove CUDA Toolkit
remove_cuda_toolkit() {
    yad --title="CUDA Removal" --width=500 --height=200 \
        --text="🔄 Removing CUDA Toolkit. Please wait..." --center --no-buttons &
    sleep 2
    kill $!

    echo "Removing CUDA Toolkit..."
    CUDA_PACKAGES=$(dpkg -l | grep 'cuda-toolkit-' | awk '{print $2}')
    for package in $CUDA_PACKAGES; do
        sudo apt-get remove -y "$package" || REMOVAL_SUCCESSFUL=false
    done
    sudo apt-get autoremove -y
    sudo apt-get autoclean
    sudo rm -rf /usr/local/cuda*
    sed -i '/cuda/d' ~/.bashrc
    source ~/.bashrc
    
    yad --title="CUDA Removal" --width=500 --height=200 \
        --text="✔️ CUDA Toolkit removal completed successfully." --button="Install CUDA":0 --center
    if [[ $? -ne 0 ]]; then exit 1; fi
    install_cuda_toolkit
}


check_nvidia_driver() {
    # Helper function to print both to the terminal and GUI
print_message() {
    local title=$1
    local message=$2
    local icon=$3
    local buttons=$4

    # Remove HTML tags for terminal output
    local terminal_message=$(echo "$message" | sed 's/<[^>]*>//g')

    # Print the message to the terminal
    echo -e "\n===== $title =====\n$terminal_message\n"

    # Display the message in the GUI
    yad --title="$title" \
        --width=500 --height=250 --text-align=center \
        --image="$icon" \
        --text="$message" \
        $buttons \
        --button="Close":1 \
        --center
}

    # Check if an NVIDIA GPU is detected
    if ! lshw -C display | grep -i nvidia &> /dev/null; then
        print_message "NVIDIA Driver Check - GPU Detection" \
            "<b>❌ No NVIDIA GPU detected in the system.</b>\n\n<b>Action Required:</b>\nPlease ensure your system has an NVIDIA GPU installed to proceed." \
            "dialog-error" 
        exit 1
    fi

    # Check if the NVIDIA kernel module is loaded
    if ! lsmod | grep -q nvidia; then
        print_message "NVIDIA Driver Check - Kernel Module" \
            "<b>❌ NVIDIA drivers are not installed or the kernel module is not loaded.</b>\n\n<b>Action Required:</b>\nPlease install the NVIDIA drivers to proceed." \
            "dialog-warning" \
            "--button=Install NVIDIA Driver:0"
        if [[ $? -ne 0 ]]; then exit 1; fi
        install_nvidia_drivers
        return 1
    fi

    # Check if `nvidia-smi` is available to verify driver installation
    if command -v nvidia-smi &> /dev/null; then
        driver_version=$(nvidia-smi | grep "Driver Version" | awk '{print $3}')
    else
        driver_version=""
    fi
    #driver_version=535.183.0
    # Check if the driver version matches the required version
    if [[ "$driver_version" == 535* ]]; then
        print_message "NVIDIA Driver Check - Driver Version" \
            "<b>✔️ Compatible NVIDIA driver version found:</b> <i>$driver_version</i>.\n\n<b>Action:</b>\nClick 'Next' to proceed to CUDA checks." \
            "dialog-information" \
            "--button=Next:0"
        if [[ $? -ne 0 ]]; then exit 1; fi
        check_cuda
    else
        print_message "NVIDIA Driver Check - Incompatible Driver" \
            "<b>⚠️ Existing NVIDIA driver version:</b> <i>$driver_version</i>.\n\n<b>Issue:</b>\nThis is not compatible with the required version <b>535</b>.\n\n<b>Action Required:</b>\nRemove the existing driver and install the correct version." \
            "dialog-warning" \
            "--button=Remove Driver:0"
        if [[ $? -ne 0 ]]; then exit 1; fi
        remove_nvidia_drivers
    fi
}

# Function to check CUDA Toolkit version
check_cuda() {
    # Helper function to print to both terminal and GUI
    print_message() {
        local title=$1
        local message=$2
        local icon=$3
        local buttons=$4

        # Remove HTML tags for terminal output
        local terminal_message=$(echo "$message" | sed 's/<[^>]*>//g')

        # Print to terminal
        echo -e "\n===== $title =====\n$terminal_message\n"

        # Show GUI dialog
        yad --title="$title" \
            --width=500 --height=250 --text-align=center \
            --image="$icon" \
            --text="$message" \
            $buttons \
            --center
    }

    # Check if CUDA Toolkit is installed
    echo "Checking for installed CUDA Toolkit..."
    local cuda_version=$(dpkg -l cuda-toolkit-[0-9]* 2>/dev/null | grep -E "^ii\s+cuda-toolkit-[0-9]+-[0-9]+" | awk '{print $3}' | cut -d. -f1-2 | head -n 1)

    if [ -z "$cuda_version" ]; then
        print_message "CUDA Check" \
            "<b>❌ CUDA Toolkit is NOT installed.</b>\n\n<b>Action Required:</b>\nPlease install the CUDA Toolkit to proceed." \
            "dialog-error" \
            "--button=Install CUDA:0 --button=Close:1"
        if [[ $? -ne 0 ]]; then
            echo "CUDA installation canceled by the user."
            exit 1
        fi
        install_cuda_toolkit
        return 1
    fi

    # CUDA version check
    if [ "$cuda_version" == "12.3" ]; then
        print_message "CUDA Check" \
            "<b>✔️ CUDA Toolkit is installed.</b>\n\n<b>Version:</b> 12.3\n\n<b>Action:</b>\nProceed with Deploying aiDAPTIV+ Middleware" \
            "dialog-information" \
            "--button=Deploy Middleware:0 --button=Close:1"
        if [[ $? -eq 0 ]]; then
            if deploy_aiDAPTIV; then
                echo ""
            else
                # If the function fails, show error dialog with yad
                yad --title "Error" \
                    --width=300 \
                    --height=100 \
                    --center \
                    --button=Close:1 \
                    --text="Error: Middleware deployment failed. Please check logs for details."
                # Exit the script with an error status
                exit 1
            fi
        else
            echo "CUDA check completed. Middleware deployment skipped."
        fi
        return 0
    else
        print_message "CUDA Check" \
            "<b>⚠️ CUDA Toolkit is installed, but the version is $cuda_version.</b>\n\n<b>Issue:</b>\nVersion $cuda_version is not compatible. Recommended version is <b>12.3</b>.\n\n<b>Action Required:</b>\nRemove the current version and install version 12.3." \
            "dialog-warning" \
            "--button=Remove CUDA:0 --button=Cancel:1"
        if [[ $? -ne 0 ]]; then
            echo "CUDA removal canceled by the user."
            exit 1
        fi
        remove_cuda_toolkit
        return 2
    fi
}



deploy_aiDAPTIV() {
    # Progress dialog while deployment starts
    (
        yad --title="Middleware Deployment" \
            --width=450 --height=200 \
            --text-align=center \
            --no-buttons \
            --image="dialog-information" \
            --text="<b>🔄 Deploying aiDAPTIV middleware. Please wait...</b>" \
            --timeout=5 \
            --timeout-indicator=top \
            --center
    ) &

    YAD_PID=$! # Capture the background dialog's process ID

    echo "===== Starting aiDAPTIV Middleware Deployment ====="

    # Update system packages
    if ! sudo apt update; then
        echo "Failed to update system packages."
        return 1
    fi

    # Install required dependencies
    if ! sudo apt install -y wget libaio1 libaio-dev liburing2 liburing-dev libboost-all-dev python3-pip libstdc++-12-dev; then
        echo "Failed to install required dependencies."
        return 1
    fi

    # Install GCC and G++ 12
    if ! sudo apt install -y gcc-12 g++-12; then
        echo "Failed to install GCC and G++ 12."
        return 1
    fi

    # Set up alternative for G++
    if ! sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 50; then
        echo "Failed to set up alternative for G++."
        return 1
    fi

    # Check pip path
    PYTHON_PATH=$(which python3 | xargs dirname)
    if [ "$PYTHON_PATH" == "" ]; then
        echo "Python3 Path is not found. Please check 'which python3' working correctly."
        return 1
    fi
    echo "Found python3 at:" $PYTHON_PATH

    # Get current user
    current_user=$(whoami)

    # Create directories
    if ! sudo mkdir -p /home/$current_user/; then
        echo "Failed to create directory /home/$current_user/."
        return 1
    fi
    if ! sudo mkdir -p /home/$current_user/Desktop/; then
        echo "Failed to create directory /home/$current_user/Desktop/."
        return 1
    fi

    # Delete old aiDAPTIV+ directories
    if ! sudo rm -rf /home/$current_user/Desktop/dm /home/$current_user/Desktop/aiDAPTIV2; then
        echo "Failed to delete old aiDAPTIV+ directories."
        return 1
    fi
    if ! sudo mkdir /home/$current_user/Desktop/dm /home/$current_user/Desktop/aiDAPTIV2; then
        echo "Failed to create new aiDAPTIV+ directories."
        return 1
    fi

    # Download aiDAPTIV+ package
    TAR_NAME="vNXUN_2_01_00.tar"
    if ! sudo rm -f $TAR_NAME; then
        echo "Failed to remove old $TAR_NAME file."
        return 1
    fi
    if ! wget --tries=3 https://phisonbucket.s3.ap-northeast-1.amazonaws.com/$TAR_NAME --no-check-certificate; then
        read -p "Can't get $TAR_NAME from cloud, Please enter the path to the $TAR_NAME file: " filepath
        if ! sudo tar xvf "$filepath" -C /home/$current_user/Desktop/aiDAPTIV2; then
            echo "Failed to extract $TAR_NAME from provided path."
            return 1
        fi
    else
        if ! sudo tar xvf $TAR_NAME -C /home/$current_user/Desktop/aiDAPTIV2; then
            echo "Failed to extract $TAR_NAME from cloud."
            return 1
        fi
    fi
    echo 'unzip package'

    # Build environment
    echo "Start to build env..."

    # Install required packages using pip
    if [[ "$PYTHON_PATH" == "/usr/bin" && "$current_user" != "root" ]]; then
        if ! yes | pip install --user -r /home/$current_user/Desktop/aiDAPTIV2/requirements.txt; then
            echo "Failed to install required packages in user space."
            return 1
        fi
        appenvpath /home/$current_user/.local/bin
    else
        if ! yes | pip install -r /home/$current_user/Desktop/aiDAPTIV2/requirements.txt; then
            echo "Failed to install required packages."
            return 1
        fi
        if [ "$current_user" == "root" ]; then
            echo "installed required packages in /usr/local/lib/python3.10/dist-packages"
        else
            echo "installed required packages in"  $(realpath $PYTHON_PATH/../lib/python3.10/site-packages/)
        fi
    fi

    # Change directory to aiDAPTIV2
    if ! cd /home/$current_user/Desktop/aiDAPTIV2; then
        return 1
    fi

    # Set executable permissions
    if ! sudo chmod +x bin/*; then
        return 1
    fi

    # Move and set permissions for phisonlib
    if ! sudo mv *.so ./phisonlib; then
        return 1
    fi
    if ! sudo chmod +x ./phisonlib/ada.exe; then
        return 1
    fi
    if ! sudo setcap cap_sys_admin,cap_dac_override=+eip ./phisonlib/ada.exe; then
        return 1
    fi

    # Move bin files to dm directory and user-local bin directory if necessary
    if [[ "$PYTHON_PATH" == "/usr/bin" && "$current_user" != "root" ]]; then
        if ! sudo cp bin/* /home/$current_user/Desktop/dm/; then
            return 1
        fi
        if ! sudo mv bin/* /home/$current_user/.local/bin/; then
            return 1
        fi
        if ! sudo rm -rf bin; then
            return 1
        fi
        echo 'Moved bin files'
        
        if ! sudo rm -rf /home/$current_user/.local/lib/python3.10/site-packages/phisonlib; then
            return 1
        fi
        if ! sudo mv phisonlib /home/$current_user/.local/lib/python3.10/site-packages/; then
            return 1
        fi
        if ! sudo rm -rf phisonlib; then
            return 1
        fi
    else
        if ! sudo cp bin/* /home/$current_user/Desktop/dm/; then
            return 1
        fi
        if ! sudo mv bin/* $PYTHON_PATH; then
            return 1
        fi
        if ! sudo rm -rf bin; then
            return 1
        fi
        echo 'Move bin files to dm/ and ' $PYTHON_PATH
        
        if [ "$current_user" == "root" ]; then
            if ! sudo rm -rf /usr/local/lib/python3.10/dist-packages/phisonlib; then
                return 1
            fi
            if ! sudo mv phisonlib /usr/local/lib/python3.10/dist-packages; then
                return 1
            fi
            if ! sudo rm -rf phisonlib; then
                return 1
            fi
        else
            if ! sudo rm -rf  $PYTHON_PATH/../lib/python3.10/site-packages/phisonlib; then
                return 1
            fi
            if ! sudo mv phisonlib $PYTHON_PATH/../lib/python3.10/site-packages/; then
                return 1
            fi
            if ! sudo rm -rf phisonlib; then
                return 1
            fi
        fi
    fi

    # echo 'export PYTHONPATH="/home/$USER/.local/lib/python3.10/site-packages/Phisonlib"' >> ~/.bashrc
    # echo 'export PYTHONPATH="/home/$USER/Desktop/aiDAPTIV2:$PYTHONPATH"' >> ~/.bashrc
    # Close progress dialog
    kill $YAD_PID 2>/dev/null

    # Step 8: Final success message
    echo "✔️ aiDAPTIV middleware deployed successfully."
    yad --title="Deployment Complete" \
        --width=450 --height=200 \
        --image="dialog-information" \
        --text="<b>✔️ Middleware deployed successfully.</b>\n" \
        --button="OK":0 --center
}



deploy_middleware() {
    local output=""
    local failed_output=""
    local success=true

    # Function to add check results to the output
    add_to_output() {
        local title=$1
        local result=$2
        local status=$3
        output+="$title:\n$result\n\n"
        if [[ $status -ne 0 ]]; then
            success=false
            failed_output+="$title:\n$result\n\n"  # Add the failed check's result to the failed_output
        fi
    }

    # Disk Space Check
    space_result=$(check_space "20G")
    add_to_output "Disk Space Check" "$space_result" $?

    # Python3 Check
    python_result=$(check_python3)
    add_to_output "Python3 Check" "$python_result" $?

    # Package Manager Check
    package_result=$(check_package_manager)
    add_to_output "Package Manager Check" "$package_result" $?

    # Output all results to the terminal
    echo -e "\n===== System Validation Results ====="
    echo -e "$output"

    # Prepare the dialog message
    local dialog_message=""
    if [[ $success == true ]]; then
        dialog_message="✅ All system checks passed successfully!"
    else
        dialog_message="❌ System validation failed:\n\n$failed_output"
    fi

    # Display results in a dialog
    if [[ $success == true ]]; then
        yad --title="System Validation" \
            --width=500 --height=300 \
            --text-align=center \
            --text="<b>$dialog_message</b>" \
            --button="Next":0 \
            --button="Close":1 \
            --center

        case $? in
            0) # Next button
                return 0
                ;;
            *) # Cancel button or window close
                echo "System validation cancelled by user."
                exit 1
                ;;
        esac
    else
        yad --title="System Validation" \
            --width=600 --height=400 \
            --text-align=left \
            --text="<b>$dialog_message</b>" \
            --button="Close":1 \
            --center

        echo -e "\nSystem validation failed. Exiting."
        exit 1
    fi
}


# Main script execution
deploy_middleware
if [[ $? -eq 0 ]]; then
    # Proceed to Nvidia driver check only if the user clicked "Next"
    check_nvidia_driver
fi

echo "===== Script Execution Complete ====="
