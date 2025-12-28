# Installing Your Custom Branch on RPi5

##  Overview

  Since you have your own branch (not PyPI package), you'll install directly from your local    
   Git clone instead of using pip install motioneye.

##  Step-by-Step Installation

 ### 1. Transfer your code to the RPi5

  **Option A:** Clone from your remote (if pushed) \
  On RPi5
  - git clone https://github.com/YOUR_USERNAME/motioneye.git
  - cd motioneye
  - git checkout update/optimize

  **Option B:** Transfer directly (if not pushed) \
  From your Windows machine
  - scp -r "C:\Users\Trent\Documents\GitHub\motioneye" pi@YOUR_RPI_IP:~/

 ### 2. Install dependencies on RPi5

  #### Update and install Python + build deps
  - sudo apt update
  - sudo apt --no-install-recommends install ca-certificates curl python3 python3-dev gcc
  - libjpeg62-turbo-dev libcurl4-openssl-dev libssl-dev

  #### Install pip
  - curl -sSfO 'https://bootstrap.pypa.io/get-pip.py'
  - sudo python3 get-pip.py
  - rm get-pip.py

  #### Fix EXTERNALLY-MANAGED restriction (Raspberry Pi OS Bookworm)
  - grep -q '\[global\]' /etc/pip.conf 2> /dev/null || printf '%b' '[global]\n' | sudo tee -a /etc/pip.conf > /dev/null
  - sudo sed -i '/^\[global\]/a\break-system-packages=true' /etc/pip.conf

 ### 3. Install motion (the backend)

  - sudo apt install motion

 ### 4. Install your branch

  #### Navigate to your cloned repo
  - cd ~/motioneye

  #### Install from local source (editable mode for easy updates)
  - sudo python3 -m pip install -e .

  #### OR non-editable install
  - sudo python3 -m pip install .

 ### 5. Initialize motionEye

  - sudo motioneye_init

  This creates config files and systemd service.

 ### 6. Start the service

  - sudo systemctl start motioneye
  - sudo systemctl enable motioneye  # auto-start on boot

 ### 7. Access the web UI

  Open browser: http://YOUR_RPI_IP:8765/
  - Username: admin
  - Password: wwadmin

 ## Updating Your Code Later

  **With editable install (-e):**
  - cd ~/motioneye
  - git pull  # or make changes
  - sudo systemctl restart motioneye

  **Without editable install:**
  - cd ~/motioneye
  - git pull
  - sudo python3 -m pip install --upgrade .
  - sudo systemctl restart motioneye

  Key Differences from Standard Install

  | Standard Install       | Your Custom Branch              |
  |------------------------|---------------------------------|
  | pip install motioneye  | pip install -e /path/to/repo    |
  | Gets code from PyPI    | Uses your local Git repo        |
  | Update via pip upgrade | Update via git pull + reinstall |
  