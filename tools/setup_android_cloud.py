"""Install the verified Android toolchain inside /workspace, without system writes."""
from pathlib import Path
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import urllib.request
import urllib.parse
import zipfile

repo=Path(__file__).resolve().parent.parent
spec=json.loads((repo/'tools/android-toolchain.json').read_text())
root=Path('/workspace/android-tools');downloads=root/'downloads';downloads.mkdir(parents=True,exist_ok=True)
sdk=Path('/workspace/android-sdk');sdk.mkdir(exist_ok=True)

def fetch(url,path,algorithm,digest):
    def valid(p):
        if not p.exists():return False
        h=hashlib.new(algorithm)
        with p.open('rb') as stream:
            for part in iter(lambda:stream.read(1048576),b''):h.update(part)
        return h.hexdigest()==digest
    if valid(path):return
    temporary=path.with_suffix(path.suffix+'.part')
    with urllib.request.urlopen(url,timeout=120) as response,temporary.open('wb') as out:shutil.copyfileobj(response,out)
    if not valid(temporary):temporary.unlink();raise RuntimeError(f'Checksum mismatch: {path.name}')
    temporary.replace(path)

def unpack(archive,destination):
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            if not (destination/item.filename).resolve().is_relative_to(destination.resolve()):raise RuntimeError('Unsafe archive path')
        z.extractall(destination)

# The managed base already contains JDK compiler modules but omits command launchers.
# Use those installed, trusted modules instead of replacing TLS trust or the JRE.
java=Path('/usr/bin/java').resolve();source=java.parent.parent
modules=subprocess.check_output([str(java),'--list-modules'],text=True)
if 'jdk.compiler@' not in modules:raise RuntimeError('A JDK 17+ with jdk.compiler is required')
jdk=root/'jdk'
if not jdk.exists():shutil.copytree(source,jdk,symlinks=True,copy_function=lambda a,b:os.symlink(a,b))
launchers={
    'java':f'exec {shlex.quote(str(java))} -Djava.home=/workspace/android-tools/jdk -Duser.home=/workspace/android-tools/home "$@"',
    'javac':'exec /workspace/android-tools/jdk/bin/java --module jdk.compiler/com.sun.tools.javac.Main "$@"',
    'jar':'exec /workspace/android-tools/jdk/bin/java --module jdk.jartool/sun.tools.jar.Main "$@"',
}
for name,command in launchers.items():
    path=jdk/'bin'/name
    if path.is_symlink():path.unlink()
    path.write_text('#!/bin/sh\n'+command+'\n');path.chmod(0o755)

archive=downloads/'commandlinetools.zip'
fetch(spec['commandToolsUrl'],archive,'sha256',spec['commandToolsSha256'])
cmd=sdk/'cmdline-tools/11.0'
if not cmd.exists():
    dest=sdk/'cmdline-tools';dest.mkdir(exist_ok=True);unpack(archive,dest);(dest/'cmdline-tools').rename(cmd)
for executable in (cmd/'bin').iterdir():executable.chmod(0o755)
gradle=root/f"gradle-{spec['gradleVersion']}"
archive=downloads/f"gradle-{spec['gradleVersion']}-bin.zip"
fetch(f"https://services.gradle.org/distributions/{archive.name}",archive,'sha256',spec['gradleSha256'])
if not gradle.exists():unpack(archive,root)
(gradle/'bin/gradle').chmod(0o755)

policy=json.loads(Path('/etc/codex/network-policy.json').read_text())
proxy=urllib.parse.urlsplit(policy['http_proxy'])
args=[str(cmd/'bin/sdkmanager'),f'--sdk_root={sdk}','--proxy=http',f'--proxy_host={proxy.hostname}',f'--proxy_port={proxy.port}']
environment=dict(os.environ,JAVA_HOME=str(jdk),ANDROID_HOME=str(sdk),ANDROID_USER_HOME='/workspace/.android')
Path('/workspace/.android').mkdir(exist_ok=True)
debug_key=Path('/workspace/.android/debug.keystore')
if not debug_key.exists():
    subprocess.run([str(source/'bin/keytool'),'-genkeypair','-keystore',str(debug_key),'-storepass','android',
        '-alias','androiddebugkey','-keypass','android','-dname','CN=MiniPet Debug,O=MiniPet,C=CL',
        '-keyalg','RSA','-keysize','2048','-validity','10000'],env=environment,check=True)
subprocess.run(args+['--licenses'],input='y\n'*100,text=True,env=environment,check=True)
subprocess.run(args+spec['sdkPackages'],env=environment,check=True)
print('Android toolchain verified and available in /workspace')
