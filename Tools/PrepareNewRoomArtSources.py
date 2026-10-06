"""Copy the new user archives into the project's portable art source directory."""
from pathlib import Path
import zipfile,json
project=Path(__file__).resolve().parents[1];incoming=project.parent
groups={'安全屋材质.zip':'SafeWall','房间材质.zip':'RoomWall','大桌子.zip':'DiningTable','煤油灯.zip':'Lantern','新书架.zip':'BookshelfNew','货架.zip':'StorageRack'}
rows=[]
for name,group in groups.items():
    archive=incoming/name
    if not archive.exists():continue
    dest=project/'ArtSource/NewRoomArt'/group;dest.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            if info.is_dir():continue
            filename=info.filename
            if not info.flag_bits & 0x800:
                try:filename=filename.encode('cp437').decode('gbk')
                except (UnicodeError,LookupError):pass
            # Only asset source files are copied; flattening avoids archive path traversal.
            base=Path(filename).name
            if Path(base).suffix.lower() not in ('.fbx','.png','.jpg','.jpeg','.exr','.tga'):continue
            target=dest/base;target.write_bytes(z.read(info))
            rows.append({'group':group,'file':str(target.relative_to(project)),'original':filename,'bytes':target.stat().st_size})
out=project/'ArtSource/NewRoomArt/Inventory.json';out.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(rows,ensure_ascii=True,indent=2))
