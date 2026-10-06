"""Import the supplied Game Jam WAV folder. Set REBORN_JAM_AUDIO_ROOT first."""
import os
import json
import subprocess
import wave
import array
from pathlib import Path
import unreal as u

source = Path(os.environ['REBORN_JAM_AUDIO_ROOT'])
out = Path(os.environ.get('REBORN_VERIFICATION_DIR', str(Path(u.Paths.project_dir()).parent / 'LocalArtifacts/GameJam_20261007')))
out.mkdir(parents=True, exist_ok=True)
names = {
    '主角压力状态缩混': 'S_PlayerPressure', '书柜倒塌_缩混': 'S_BookCollapse',
    '停电': 'S_PowerDown', '停电2跳闸': 'S_Breaker', '关门': 'S_DoorClose',
    '安全屋待机': 'S_SafeIdle', '怪物叫声 （第一阶段房间二）': 'S_FirstRoar',
    '怪物叫声': 'S_FinalRoar', '怪物攻击': 'S_MonsterAttack', '怪物笑声': 'S_MonsterLaugh',
    '怪脚 ': 'S_MonsterSteps', '拉开门': 'S_DoorOpen', '拾取物品': 'S_Pickup',
    '撞柜子': 'S_CabinetImpact', '柜子拖拽': 'S_CabinetDrag', '标题弹出': 'S_TitleReveal',
    '灯闪烁': 'S_LampFlicker', '白光': 'S_WhiteFlash', '脚步': 'S_PlayerSteps',
    '脚步二阶段': 'S_Stage2Steps', '环境背景音_缩混': 'S_Ambient',
    '第一阶段怪物登场缩混': 'S_MonsterEntrance',
    '第二阶段低压力恐怖氛围音乐': 'S_Stage2Music', '追逐音乐': 'S_ChaseMusic',
}
loops = {'S_PlayerPressure', 'S_SafeIdle', 'S_Ambient', 'S_Stage2Music', 'S_ChaseMusic'}
tasks = []
converted = out / 'CompatibleAudio'
converted.mkdir(exist_ok=True)
for file in source.rglob('*.wav'):
    assert file.stem in names, file.name
    task = u.AssetImportTask()
    compatible = converted / (names[file.stem] + '.wav')
    subprocess.run([os.environ['REBORN_FFMPEG'], '-y', '-loglevel', 'error', '-i', str(file), '-ar', '48000', '-ac', '2', '-c:a', 'pcm_s16le', str(compatible)], check=True)
    task.filename = str(compatible)
    task.destination_path = '/HorrorSystems/Audio/Jam'
    task.destination_name = names[file.stem]
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)
assert len(tasks) == len(names), 'The supplied sound folder is incomplete'
for full, single in [('S_PlayerSteps','S_PlayerStepSingle'),('S_Stage2Steps','S_Stage2StepSingle'),('S_MonsterSteps','S_MonsterStepSingle')]:
    with wave.open(str(converted/(full+'.wav')),'rb') as w:
        params=w.getparams(); data=array.array('h',w.readframes(w.getnframes()))
    window=int(params.framerate*.2)*params.nchannels
    peak=max(range(0,max(1,len(data)-window),window),key=lambda i:sum(v*v for v in data[i:i+window]))
    begin=max(0,peak-int(params.framerate*.05)*params.nchannels)
    begin-=begin%params.nchannels
    with wave.open(str(converted/(single+'.wav')),'wb') as w:
        w.setparams(params);w.writeframes(data[begin:begin+int(params.framerate*.4)*params.nchannels].tobytes())
    task=u.AssetImportTask();task.filename=str(converted/(single+'.wav'));task.destination_path='/HorrorSystems/Audio/Jam';task.destination_name=single
    task.automated=True;task.replace_existing=True;task.save=True;tasks.append(task)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
report = []
for task in tasks:
    asset = u.EditorAssetLibrary.load_asset(task.destination_path + '/' + task.destination_name)
    assert isinstance(asset, u.SoundWave), task.destination_name
    asset.set_editor_property('looping', task.destination_name in loops)
    asset.set_editor_property('compression_quality', 60)
    assert u.EditorAssetLibrary.save_loaded_asset(asset)
    report.append({'source': task.filename, 'asset': asset.get_path_name(), 'looping': task.destination_name in loops})
(out / 'JamAudioImport.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
u.log('REBORN_JAM_AUDIO_IMPORTED ' + str(len(report)))
