# Local development

## Simulator

From a clone of this repository:

```bash
cd simulator
npm install
npm run dev
```

Useful checks:

```bash
cd simulator
npm run lint
npm run test
npm run build
```

## Zephyr firmware

Because this repository is a West manifest repository, initialize a Zephyr workspace around it before building firmware.

```bash
mkdir -p ~/zephyr-workspaces/imu_sandbox
cd ~/zephyr-workspaces/imu_sandbox
git clone git@github.com:mpolaczyk/imu_sandbox.git app
python3 -m venv .venv
source .venv/bin/activate
pip install west
west init -l app
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt
west build -s app/firmware/blinky -b xiao_ble/nrf52840/sense
```

If you already have a Zephyr workspace and toolchain, you can instead add this repository as the manifest repo and build the same application path directly.
