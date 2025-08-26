start_time=$(date +%s)
time=$(date +%Y-%m-%d-%H-%M)
mkdir -p output/config
mkdir -p output/tmp
sudo dmidecode -t memory > output/config/dmidecode.out
cp config.ini output/config/
sudo modprobe msr
sudo wrmsr -a 0x1A4 0xF
sudo taskset 0x1 ./bin/main
end_time=$(date +%s)