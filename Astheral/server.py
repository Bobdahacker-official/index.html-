"""Astheral local server. Python 3.10+, no packages required.
Optional server LLM: ASTHERAL_API_URL=https://your-provider/v1/chat/completions
ASTHERAL_API_KEY stays server-side. Set ASTHERAL_CHAT_MODEL / ASTHERAL_CODE_MODEL.
Default binding is loopback; no open, unauthenticated public proxy is created.
"""
import http.server, json, os, pathlib, urllib.request, urllib.error
ROOT=pathlib.Path(__file__).resolve().parent
ENDPOINT=os.environ.get('ASTHERAL_API_URL','')
KEY=os.environ.get('ASTHERAL_API_KEY','')
MODELS={'chat':os.environ.get('ASTHERAL_CHAT_MODEL',''),'code':os.environ.get('ASTHERAL_CODE_MODEL','')}
class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*args,**kwargs): super().__init__(*args,directory=str(ROOT),**kwargs)
    def do_GET(self):
        if self.path.split('?')[0]=='/server-config.json':
            self.send_json(200,{'enabled':bool(ENDPOINT and all(MODELS.values())), 'chat_model':MODELS['chat'],'code_model':MODELS['code']});return
        # Serve only browser assets; never expose training scripts, Git state, or environment files.
        path=self.path.split('?')[0]
        if path not in ['/', '/index.html','/styles.css','/app.js','/core.js','/skills.js','/models.json','/llm-worker.js','/favicon.ico']:
            self.send_error(404);return
        super().do_GET()
    def do_POST(self):
        if self.path!='/api/chat': self.send_error(404);return
        # Browser requests must originate at this exact local server.
        origin=self.headers.get('Origin')
        if origin and origin not in ['http://'+self.headers.get('Host','')]: self.send_error(403);return
        if not ENDPOINT or not all(MODELS.values()): self.send_json(503,{'error':'Model server is not configured.'});return
        try:
            length=int(self.headers.get('Content-Length','0'))
            if length<1 or length>100000: self.send_error(413);return
            body=json.loads(self.rfile.read(length))
            mode=body.get('mode','chat');messages=body.get('messages')
            if mode not in MODELS or not isinstance(messages,list) or len(messages)>100: raise ValueError('Invalid messages or mode')
            if any(not isinstance(m,dict) or m.get('role') not in ['system','user','assistant'] or not isinstance(m.get('content'),str) for m in messages): raise ValueError('Invalid message format')
            payload=json.dumps({'model':MODELS[mode],'messages':messages,'stream':True,'max_tokens':2048 if mode=='code' else 1024,'temperature':.2 if mode=='code' else .7}).encode()
            headers={'Content-Type':'application/json','Accept':'text/event-stream'}
            if KEY: headers['Authorization']='Bearer '+KEY
            request=urllib.request.Request(ENDPOINT,data=payload,headers=headers,method='POST')
            upstream=urllib.request.urlopen(request,timeout=120)
        except (ValueError,TypeError,json.JSONDecodeError): self.send_json(400,{'error':'Invalid request.'});return
        except (urllib.error.URLError,OSError): self.send_json(502,{'error':'Model provider is unavailable. Check server settings.'});return
        self.send_response(200);self.send_header('Content-Type',upstream.headers.get('Content-Type','text/event-stream'));self.send_header('Cache-Control','no-store');self.end_headers()
        try:
            with upstream:
                for line in upstream:
                    self.wfile.write(line);self.wfile.flush()
        except (BrokenPipeError,ConnectionResetError): pass
    def send_json(self,status,data):
        content=json.dumps(data).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
if __name__=='__main__':
    port=int(os.environ.get('PORT','8000'))
    http.server.ThreadingHTTPServer(('127.0.0.1',port),Handler).serve_forever()
