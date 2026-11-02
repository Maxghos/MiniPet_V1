"""Build/verify an indexed Arduino ESP32 CA bundle from a checksum-pinned wheel."""
from pathlib import Path
import argparse,hashlib,json,struct,urllib.request,zipfile,io,re

ROOT=Path(__file__).resolve().parent.parent
URL='https://files.pythonhosted.org/packages/0b/a7/71ac2cff56fec219ed242bb11b8efb69fcc4bec75db06fb7bfe35de520e6/certifi-2026.7.22-py3-none-any.whl'
WHEEL_SHA='62f22742b58a1a33014a2b6b706588a8d7e2a88ae7bd1a6ebe8c992928483775'
PEM_SHA='9cc2a774b5198dcff14d9be1e66091f538975d867ce029a96bce15a55dfd730f'
parser=argparse.ArgumentParser();parser.add_argument('--wheel');parser.add_argument('--check',action='store_true');args=parser.parse_args()
if args.check:
    metadata=json.loads((ROOT/'certs/telegram_roots.json').read_text())
    raw=(ROOT/'certs/telegram_roots.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest()==metadata['bundle_sha256']
    count=struct.unpack_from('>H',raw)[0];assert count==metadata['roots'] and 1<=count<=256
    offset=2;subjects=[]
    for _ in range(count):
        names,keys=struct.unpack_from('>HH',raw,offset);offset+=4
        assert names and keys and offset+names+keys<=len(raw)
        subjects.append(raw[offset:offset+names]);offset+=names+keys
    assert offset==len(raw) and subjects==sorted(subjects) and len(set(subjects))==count
    print(f'Telegram public CA bundle verified: {count} roots')
else:
    from cryptography import x509
    from cryptography.hazmat.primitives.serialization import Encoding,PublicFormat
    raw=Path(args.wheel).read_bytes() if args.wheel else urllib.request.urlopen(URL,timeout=30).read()
    assert hashlib.sha256(raw).hexdigest()==WHEEL_SHA,'Wheel checksum mismatch'
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        pem=archive.read('certifi/cacert.pem')
        license_name=next(n for n in archive.namelist() if n.endswith('/licenses/LICENSE') or n.endswith('.dist-info/LICENSE'))
        license_text=archive.read(license_name)
    assert hashlib.sha256(pem).hexdigest()==PEM_SHA,'CA source checksum mismatch'
    entries={}
    for block in re.findall(rb'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----',pem,re.S):
        cert=x509.load_pem_x509_certificate(block)
        entries[cert.subject.public_bytes()]=cert.public_key().public_bytes(Encoding.DER,PublicFormat.SubjectPublicKeyInfo)
    bundle=struct.pack('>H',len(entries))
    for subject,key in sorted(entries.items()):bundle+=struct.pack('>HH',len(subject),len(key))+subject+key
    (ROOT/'certs/telegram_roots.bin').write_bytes(bundle)
    (ROOT/'certs/CERTIFI_LICENSE.txt').write_bytes(license_text)
    (ROOT/'certs/telegram_roots.json').write_text(json.dumps(dict(source='Mozilla CA store, certifi 2026.7.22',wheel=URL,
        wheel_sha256=WHEEL_SHA,certifi_pem_sha256=PEM_SHA,roots=len(entries),bundle_sha256=hashlib.sha256(bundle).hexdigest()),indent=2)+'\n')
    print(f'Generated CA bundle: {len(entries)} roots, {len(bundle)} bytes')
