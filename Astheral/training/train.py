"""Train TWO independent neural softmax skill classifiers; no network or GPU.
Run: OPENBLAS_NUM_THREADS=1 python training/train.py
These weights route local skills. They are not Qwen language-model weights.
"""
import hashlib, json, math, pathlib, re, time
import numpy as np
ROOT=pathlib.Path(__file__).resolve().parents[1]

def tokenize(text):
    words=re.findall(r"[a-z0-9]+", text.lower())
    return words + [words[i]+'_'+words[i+1] for i in range(len(words)-1)]

def train(mode, classes):
    started=time.perf_counter()
    vocab=sorted({t for c in classes for s in c['train'] for t in tokenize(s)})
    index={w:i for i,w in enumerate(vocab)}
    def features(text):
        x=np.zeros(len(vocab))
        for t in tokenize(text):
            if t in index: x[index[t]]+=1
        n=np.linalg.norm(x)
        return x/n if n else x
    samples=[(s,i) for i,c in enumerate(classes) for s in c['train']]
    x=np.array([features(s) for s,_ in samples]); y=np.array([i for _,i in samples])
    rng=np.random.default_rng(42 if mode=='chat' else 84)
    w=rng.normal(0,.01,(len(vocab),len(classes))); b=np.zeros(len(classes))
    losses=[]
    for epoch in range(1600):
        z=x@w+b; z-=z.max(axis=1,keepdims=True)
        p=np.exp(z); p/=p.sum(axis=1,keepdims=True)
        loss=float(-np.log(p[np.arange(len(y)),y]+1e-12).mean())
        if epoch in [0,99,399,799,1599]: losses.append({'epoch':epoch+1,'loss':round(loss,6)})
        p[np.arange(len(y)),y]-=1; p/=len(y)
        w-=2.2*(x.T@p+0.00008*w); b-=2.2*p.sum(axis=0)
    heldout=[(s,i) for i,c in enumerate(classes) for s in c['test']]
    def accuracy(rows):
        return sum(int(np.argmax(features(s)@w+b)==i) for s,i in rows)/len(rows)
    predictions=[{'text':s,'expected':classes[i]['id'],'predicted':classes[int(np.argmax(features(s)@w+b))]['id']} for s,i in heldout]
    report={'mode':mode,'architecture':'L2-normalized unigram/bigram input → learned dense softmax classifier',
      'seed':42 if mode=='chat' else 84,'epochs':1600,'train_examples':len(samples),'heldout_examples':len(heldout),
      'features':len(vocab),'parameters':int(w.size+b.size),'train_accuracy':accuracy(samples),'heldout_accuracy':accuracy(heldout),
      'loss_history':losses,'heldout_predictions':predictions,'elapsed_seconds':round(time.perf_counter()-started,3)}
    model={'vocabulary':vocab,'labels':[c['id'] for c in classes],'weights':np.round(w,6).tolist(),'bias':np.round(b,6).tolist()}
    return model,report

def main():
    dataset_path=ROOT/'training/dataset.json'
    data=json.loads(dataset_path.read_text())
    models={}; reports=[]
    for mode in ['chat','code']:
        models[mode],report=train(mode,data[mode]); reports.append(report)
        print(f"{mode}: {report['train_examples']} training / {report['heldout_examples']} held-out; accuracy {report['heldout_accuracy']:.1%}; loss {report['loss_history'][0]['loss']} → {report['loss_history'][-1]['loss']}")
    (ROOT/'models.json').write_text(json.dumps(models,separators=(',',':')))
    report={'training_scope':'Two newly trained local skill classifiers. Pretrained Qwen generation weights are unchanged.',
      'dataset_sha256':hashlib.sha256(dataset_path.read_bytes()).hexdigest(),'runs':reports}
    (ROOT/'training/report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__': main()
