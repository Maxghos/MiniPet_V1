"""Run the same Gradle checks used locally; export APK outside the checkout."""
from pathlib import Path
import hashlib,json,os,subprocess,urllib.parse,shutil
repo=Path(__file__).resolve().parent.parent
policy=json.loads(Path('/etc/codex/network-policy.json').read_text())
proxy=urllib.parse.urlsplit(policy['http_proxy'])
environment=dict(os.environ,JAVA_HOME='/workspace/android-tools/jdk',ANDROID_HOME='/workspace/android-sdk',ANDROID_USER_HOME='/workspace/.android',GRADLE_USER_HOME='/workspace/.gradle')
args=['/workspace/android-tools/gradle-8.9/bin/gradle','-p',str(repo/'android'),'--no-daemon',f'-Dhttps.proxyHost={proxy.hostname}',f'-Dhttps.proxyPort={proxy.port}',f'-Dhttp.proxyHost={proxy.hostname}',f'-Dhttp.proxyPort={proxy.port}',':app:testDebugUnitTest',':app:lintDebug',':app:assembleDebug']
subprocess.run(args,env=environment,check=True)
source=repo/'android/app/build/outputs/apk/debug/app-debug.apk'
exports=Path('/workspace/artifacts');exports.mkdir(exist_ok=True)
apk=exports/'MiniPet-3.3.0-debug.apk';shutil.copy2(source,apk)
checksum=hashlib.sha256(apk.read_bytes()).hexdigest()
(apk.with_suffix('.apk.sha256')).write_text(f'{checksum}  {apk.name}\n')
print(f'APK exported outside Git: {apk}')
