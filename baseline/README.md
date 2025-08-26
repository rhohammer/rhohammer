# Zenhammer

## zenhammer

### step 1

cd zenhammer,exec the following orders:

```bash
mkdir build && \
cd build && \
cmake .. && \
make -j$(nproc)
```

### step 2

16G:

```bash
sudo ./zenHammer --dimm-id 1 --runtime-limit 21600 --geometry 2,4,4 --samsung --sweeping | tee out.log
```

8G:

```bash
sudo ./zenHammer --dimm-id 1 --runtime-limit 21600 --geometry 2,2,4 --sweeping | tee out.log
```

### step 3

see stdout.log


## replay all

after ##zenhammer,you can find the file "fuzz-summary.json",then run the following code to replay all the patterns in the file.

```bash
sudo ./zenHammer --dimm-id 1 --geometry 2,4,4 --samsung -j fuzz-summary.json --sweeping
```


## replay some

if you want replay one or several patterns, you can run the code like this:

```bash
sudo ./zenHammer --dimm-id 1 --geometry 2,4,4 --samsung -j fuzz-summary.json -y 9166b3d5-6b36-4f2d-9c80-306b075ae72f  --sweeping
```

replace "9166b3d5-6b36-4f2d-9c80-306b075ae72f" with your pattern ids, using comma between them, like this:
"9166b3d5-6b36-4f2d-9c80-306b075ae72f,9166b3d5-6b36-4f2d-9c80-306b075ae72f,9166b3d5-6b36-4f2d-9c80-306b075ae72f"
