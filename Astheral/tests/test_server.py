"""Isolated HTTP integration tests. No external model or API key needed."""
import http.server, importlib.util, json, pathlib, threading, unittest, urllib.request, urllib.error
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('astheral_server',ROOT/'server.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class ServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server=http.server.ThreadingHTTPServer(('127.0.0.1',0),module.Handler)
        cls.thread=threading.Thread(target=cls.server.serve_forever,daemon=True);cls.thread.start()
        cls.origin='http://127.0.0.1:'+str(cls.server.server_port)
    @classmethod
    def tearDownClass(cls): cls.server.shutdown();cls.server.server_close();cls.thread.join()
    def request(self,path,data=None,origin=None):
        return urllib.request.urlopen(urllib.request.Request(self.origin+path,data=data,headers={'Content-Type':'application/json',**({'Origin':origin} if origin else {})}),timeout=3)
    def test_browser_assets(self):
        for path in ['/','/app.js','/styles.css','/models.json','/llm-worker.js']:
            with self.request(path) as r:self.assertEqual(r.status,200)
    def test_config_discloses_no_secret(self):
        with self.request('/server-config.json') as r:config=json.load(r)
        self.assertFalse(config['enabled']);self.assertNotIn('key',config)
    def test_private_files_not_served(self):
        for path in ['/.env','/server.py','/../server.py','/.git/config']:
            with self.assertRaises(urllib.error.HTTPError) as e:self.request(path)
            self.assertEqual(e.exception.code,404)
    def test_unconfigured_endpoint_fails_explicitly(self):
        with self.assertRaises(urllib.error.HTTPError) as e:self.request('/api/chat',b'{}',self.origin)
        self.assertEqual(e.exception.code,503)
    def test_cross_origin_request_is_rejected(self):
        with self.assertRaises(urllib.error.HTTPError) as e:self.request('/api/chat',b'{}','https://untrusted.example')
        self.assertEqual(e.exception.code,403)
if __name__=='__main__':unittest.main()
