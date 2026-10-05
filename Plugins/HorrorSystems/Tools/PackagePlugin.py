"""Create a source/content-only transferable plugin ZIP; never includes build caches."""
from pathlib import Path
import zipfile, hashlib, json
plugin=Path(__file__).resolve().parent.parent
project=plugin.parent.parent
dest=project/"Portable"
dest.mkdir(exist_ok=True)
archive=dest/"HorrorSystems_Source.zip"
exclude={"Binaries","Intermediate","Saved","__pycache__"}
entries=[]
with zipfile.ZipFile(archive,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for path in sorted(plugin.rglob("*")):
        relative=path.relative_to(plugin)
        if not path.is_file() or any(part in exclude for part in relative.parts): continue
        z.write(path,Path("HorrorSystems")/relative); entries.append(str(relative))
sha=hashlib.sha256(archive.read_bytes()).hexdigest()
(dest/"Manifest.json").write_text(json.dumps({"archive":archive.name,"engine":"5.8.2","sha256":sha,"files":len(entries),"bytes":archive.stat().st_size},indent=2),encoding="utf-8")
print("Portable plugin:",archive,"files:",len(entries),"SHA256:",sha)
