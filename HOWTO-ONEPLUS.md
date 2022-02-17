원플(Oneplus 3t)에서 오픈파일럿 이식하려면?
------
1. 일단 /data 디렉토리에서 0.8.9 브렌치를 설치해야 상위 버전으로 이동할 수 있습니다.
```
cd /data/ && mv openpilot openpilot-backup;
git clone https://github.com/kans-ky/ona8.git openpilot --branch 089-oneplus3
```
0.8.9를 클론했다면, 3번으로 이동합니다.

2. 0.8.9이상 버전에서는 바로 0.8.13 브렌치를 클론합니다.
```
cd /data/ && mv openpilot openpilot-backup;
git clone https://github.com/kans-ky/opa8.git openpilot --branch Volt-oneplus3
```
3. 아래 명령을 수행해줍니다. 
```
chmod 700 /data/openpilot/unix.sh;
chmod 700 /data/openpilot/scripts/oneplus_update_neos.sh;
cd /data/openpilot/ && ./unix.sh; 
cd /data/openpilot/scripts/ && ./oneplus_update_neos.sh
```

3. 원플용 Neos(recovery, system 이미지)가 다운로드되면 자동으로 부팅됩니다. 
   두번 정도 리부팅되고 나면 fastboot 모드로 들어갑니다.

4. fastboot 모드에서 볼륨 상하버튼으로 리커버리모드 선택한 뒤 파워버튼으로 실행합니다. 

5. 리커버리 모드에서 차례대로 apply update` -> `Choose from emulated` -> `0/` -> `update.zip` -> `Reboot system now` 선택합니다.
   원플용 0.8.9버전으로 부팅됩니다. 
   긴 빌드과정이(15분이상) 끝나면 0.8.9버전으로 부팅됩니다. 
   터치 반응이 없으므로 강제부팅합니다.

6. 0.8.9부팅이 성공했다면, 바로 0.8.13브렌치를 클론합니다.
   0.7.x버전과 0.8.x 이온접속 개인키 변경법은 생략합니다.
   (0.8.13으로 클론후 부팅했다면 아래 부분은 모두 건너뜁니다.)
```
cd /data/ && mv openpilot openpilot-089;
git clone https://github.com/kans-ky/opa8.git openpilot --branch Volt-oneplus3
```
7. 0.8.13 클론후 바로 부팅하지 않고, 아래 명령을 실행합니다.
```
chmod 700 /data/openpilot/unix.sh;
cd /data/openpilot/ && ./unix.sh;
reboot;
```

8. 마찬가지로 긴 빌드과정을 거치고 나면 0.8.13으로 부팅됩니다.
   성공하시기를 바랍니다.
