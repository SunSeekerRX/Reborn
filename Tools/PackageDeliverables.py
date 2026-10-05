"""Archive clean project sources and the cooked Windows game into this project folder."""
from pathlib import Path
import os, zipfile, hashlib, json, shutil

project=Path(__file__).resolve().parent.parent
dest=project/"Deliverables"
game=dest/"WindowsGame"
assert (game/"Reborn.exe").is_file(),"Windows package has not finished"
dest.mkdir(exist_ok=True)
excluded={"Binaries","Intermediate","Saved","DerivedDataCache","Portable",".git",".vs","__pycache__","Deliverables","Archive"}

def archive(root,output,prefix,exclude=()):
    count=0
    with zipfile.ZipFile(output,"w",zipfile.ZIP_DEFLATED,compresslevel=3) as z:
        for base,dirs,files in os.walk(root):
            dirs[:]=sorted(d for d in dirs if d not in exclude)
            for name in sorted(files):
                path=Path(base)/name
                if path.suffix in (".sln",".suo",".obj",".pdb"): continue
                z.write(path,Path(prefix)/path.relative_to(root)); count+=1
    with zipfile.ZipFile(output) as z:
        assert z.testzip() is None,"Archive CRC failed"
    digest=hashlib.sha256()
    with output.open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""): digest.update(chunk)
    return {"file":output.name,"files":count,"bytes":output.stat().st_size,"sha256":digest.hexdigest()}

print("Archiving project sources...",flush=True)
source_info=archive(project,dest/"Reborn_Project.zip","Reborn",excluded)
print("Archiving Windows Shipping game...",flush=True)
game_info=archive(game,dest/"Reborn_Windows.zip","RebornWindows")
verification=dest/"Verification"; verification.mkdir(exist_ok=True)
for name in ("ContentAudit.json","Automation/index.json","Packaging.log","RuntimeTests.log","PackagedBoot.json","RenameVerification.json","Gameplay.png","Inspection.png","RoomContent.json","WindowMonsterLook.png","RoomSafe.png","FirstPersonPickup.png","RoomInspection.png","RoomTimeoutReset.png","EndingWhiteFlash.png","GameEnding.png"):
    source=project/"Saved/Verification"/name
    if source.is_file(): shutil.copy2(source,verification/source.name)
(dest/"Manifest.json").write_text(json.dumps({"name":"Reborn","version":"0.5.0","engine":"5.8.2","tests_passed":7,"platform":"Win64","configuration":"Shipping","project":source_info,"game":game_info},indent=2),encoding="utf-8")
print(json.dumps({"project":source_info,"game":game_info},ensure_ascii=False),flush=True)
