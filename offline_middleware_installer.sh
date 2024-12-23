#!/bin/bash

# Start logging
echo "===== Starting System Validation ====="
echo "Timestamp: $(date)"
echo "======================================"

# Log file
LOG_FILE="aiDAPTIV_Middleware_Deployment.log"

# Try creating or appending to the log file
if ! exec > >(tee -a "$LOG_FILE") 2>&1; then
    echo "Permission denied: Unable to create or write to the log file $LOG_FILE. Please check the permissions."
    exit 1
fi


current_dir=$(pwd)

# To cleanup installation cache
cleanup(){
    # Purge packages in problematic states
    dpkg -l | grep -E '^rc|^iU|^iF|^hF' | awk '{print $2}' | xargs -r sudo dpkg --purge >/dev/null 2>&1
    sudo dpkg -l | grep -E '^(rc|iU|iF|hF)' | awk '{print $2}' | xargs -r sudo dpkg --remove --force-remove-reinstreq >/dev/null 2>&1
    # Cleaning up from /var/cache/apt/archives
    sudo apt-get clean
    # Handle pip cleanup if pip is installed
    if command -v pip &>/dev/null 2>&1; then
        pip cache purge &>/dev/null 2>&1
    fi
    echo "Exiting....."
}

#Trap EXIT signals
trap cleanup EXIT


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


remove_cuda_toolkit() {
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
    
}


# List of APT packages to check
apt_packages=(
    "cuda-toolkit-12-3"
    "g++-12" "gcc-12"
    "libaio1"
    "libaio-dev"
    "libboost-all-dev"
    "libstdc++-12-dev"
    "liburing2"
    "liburing-dev"
    "lvm2"
    "mdadm"
    "nvidia-driver-535"
    "python3-pip"
    "vim"
    "wget"
)


# Function to check if required APT packages are installed
check_apt_packages() {
    for package in "${apt_packages[@]}"; do
        if ! sudo dpkg -l "$package" &>/dev/null; then
            return 1
        fi
    done
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

    yad --title="Removal Complete" \
        --width=400 --height=200 \
        --image="dialog-information" \
        --text="<b>✔️ NVIDIA drivers successfully removed.</b>\n\nInstall NVIDIA drivers and other packages now." \
        --button="Install":0 \
        --button="Close":1 \
        --center

    # Handle user decision
    if [[ $? -eq 0 ]]; then
        install_deb_packages
    fi
}


# Function to install APT packages
install_deb_packages() {
    # Path where all .deb files are available
    local deb_dir="required_packages/apt_packages/all_debs"
    #Storing the current directory location.
    local current_dir=$(pwd)

    echo -e "********** Starting Driver and other package Installation **********"
    if [ ! -d "$deb_dir" ]; then
        echo "✘ Error: Directory '$deb_dir' not found..."
        return 1
    fi

    cd "$deb_dir" || { echo "Error: Failed to enter directory $deb_dir"; return 1; }

    #install all deb files forcefully. (to override existing packages)
    if ! sudo dpkg -i --force-all --auto-deconfigure *.deb; then
        echo "✘ Error: Installation of APT packages failed.. Try to Deploy again..."
        return 1
    fi

    echo "✓ ********** APT packages installation completed successfully **********"
    cd "$current_dir"

        # Success message
    echo "Please reboot the system to take effect."
    yad --title="Installation Complete" \
        --width=400 --height=200 \
        --image="dialog-information" \
        --text="<b>✔️ Installation successfull.</b>\n\nPlease reboot the system to apply changes." \
        --button="Reboot Now":0 \
        --button="Close":1 \
        --center

    # Handle reboot decision
    if [[ $? -eq 0 ]]; then
        echo "Rebooting system..."
        sudo reboot now
    fi
}


check_driver() {
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

    if ! lsmod | grep -q nvidia; then
        print_message "NVIDIA Driver Check" \
            "<b>❌ NVIDIA drivers are not installed.</b>\n\n<b>Action Required:</b>\nPlease install the NVIDIA drivers and other required packages." \
            "dialog-warning" \
            "--button=Install NVIDIA Driver:0"
        if [[ $? -ne 0 ]]; then exit 1; fi
        install_deb_packages
        return 1 
    fi


    if command -v nvidia-smi &> /dev/null; then
        driver_version=$(nvidia-smi | grep "Driver Version" | awk '{print $3}')
    else
        driver_version=""
    fi

    #after reboot
    if [[ "$driver_version" == 535* ]]; check_apt_packages; then
        echo "Required Drivers (535.183) and other packages are found.. Proceed with deploying middleware..."
        print_message "Drivers Found" \
            "<b>✔️ Required Drivers and other packages are found.</b>\n<b>Action:</b>\nProceed with Deploying aiDAPTIV+ Middleware" \
            "dialog-information" \
            "--button=Deploy Middleware:0"
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
        fi
    else
        print_message "NVIDIA Driver Check - Incompatible Driver" \
        "<b>⚠️ Existing NVIDIA driver found or some required packages are missing.</b>\n <b>Action Required:</b>\nRemove the existing driver and install again." \
        "dialog-warning" \
        "--button=Remove Driver:0"
        if [[ $? -ne 0 ]]; then exit 1; fi
        remove_nvidia_drivers   
    fi



}


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


# Updated function to install Python packages
install_python_packages() {
    PYTHON_PATH=$(which python3 | xargs dirname)
    echo "Found python3 at:"$PYTHON_PATH

    if [ "$PYTHON_PATH" == "" ]; then
        echo "✘ Python3 Path is not found. Please check 'which python3' working correctly."
        return 1
    fi

    current_user=$(whoami)
    echo -e "********** Starting Python Package Installation **********"
    #function to check if pip is installed
    local current_dir=$(pwd)

    if [[ "$PYTHON_PATH" == "/usr/bin" && "$current_user" != "root" ]]; then
        local base_dir="required_packages/python_packages/all_wheels"
        echo "**********Starting Python package installation (user-level)**********"
        cd "$base_dir" || { echo "Failed to enter base directory $base_dir"; return 1; }
        if ! sudo -u $current_user python3 -m pip install --user --no-index --force-reinstall --find-links=. *.whl; then
            echo "✘ Error: Installation of Python packages failed."
            return 1
        fi
        appenvpath /home/$current_user/.local/bin
        echo "✓ User-level installation completed successfully."

    else
        if [ "$current_user" == "root" ]; then
            echo "Installing required packages in /usr/local/lib/python3.10/dist-packages"
        else
            echo "Installing required packages in $(realpath $PYTHON_PATH/../lib/python3.10/site-packages/)"
        fi

        local base_dir="required_packages/python_packages/all_wheels"
        echo "**********Starting Python package installation (system-wide)**********"
        cd "$base_dir" || { echo "Failed to enter base directory $base_dir"; return 1; }

        if ! pip install --no-index --force-reinstall --find-links=. *.whl; then
            echo "✘ Error: Installation of Python packages failed."
            return 1
        fi
        echo "✓ System-wide installation completed successfully."
    fi
    cd "$current_dir"
}


# Function to set up the aiDAPTIV2 package
setup_aidaptiv_package()
{
    cd $current_dir
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
    TAR_NAME="middleware/vNXUN_2_01_00.tar"
    if ! sudo tar xvf $TAR_NAME -C /home/$current_user/Desktop/aiDAPTIV2; then
        echo "Failed to extract $TAR_NAME."
        return 1
    fi
    echo 'unzip package'    
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
            --timeout=2 \
            --timeout-indicator=top \
            --center
    ) &

    YAD_PID=$! # Capture the background dialog's process ID

    echo "===== Starting aiDAPTIV Middleware Deployment ====="

    if ! setup_aidaptiv_package; then 
        return 1
    fi

    if ! install_python_packages; then
        return 1
    fi

    # Get current user
    current_user=$(whoami)

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
    check_driver
fi

echo "===== Script Execution Complete ====="

