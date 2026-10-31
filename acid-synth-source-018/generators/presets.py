"""Device presets from definition.json, with the SDK's neutral host settings."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[1]
NAMES={'Acid Spark':'default','Lime Phreak':'lime_phreak','Neon Bass':'neon_bass',
       'V7ACID':'v7acid','Acid Rush':'acid_rush'}

def documents():
    definition=json.loads((ROOT/'definition.json').read_text())
    result={}
    for name,filename in NAMES.items():
        patch=definition['presets'][name]
        result[filename]={
            'version':4,'platform':'OP-XY','type':'acid','regions':[],
            'engine':{
                'bendrange':8191,'detune':0,'highpass':0,
                'modulation':{name:{'amount':16384,'target':0} for name in ['aftertouch','modwheel','pitchbend','velocity']},
                'params':[round(patch[p['id']]*32767) for p in definition['parameters']],
                'playmode':'mono','portamento.amount':0,'portamento.type':0,'transpose':0,
                'tuning.root':0,'tuning.scale':0,'velocity.sensitivity':0,'volume':32767,'width':0,
            },
            'envelope':{'amp':{'attack':128,'decay':8192,'sustain':32767,'release':32767},
                        'filter':{'attack':0,'decay':0,'release':0,'sustain':32767}},
            'fx':{'active':False,'params':[0,0,0,0,0,32767,0,0],'type':'svf'},
            'lfo':{'active':False,'params':[0]*8,'type':'value'},'octave':0,
        }
    return result

if __name__=='__main__':
    for name,document in documents().items():
        (ROOT/'presets'/(name+'.json')).write_text(json.dumps(document,indent=2)+'\n')
