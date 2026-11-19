import os
import io, random, subprocess, numpy as np, time
from PIL import Image
import mozjpeg_lossless_optimization as mz
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test.py')).read().split('fails = 0')[0])
random.seed(7)
seeds=[]
for (w,h,sub) in [(200,136,2),(200,136,0),(280,187,1),(64,48,2)]:
    img=photo(w,h,w+h+sub)
    for prog in (False,True):
        b=io.BytesIO(); img.save(b,'JPEG',quality=85,subsampling=sub,progressive=prog); seeds.append(b.getvalue())
    seeds.append(mz.optimize(seeds[-2]))
crashes=0; n=0; t0=time.time(); slow=0
for i in range(8000):
    s=bytearray(random.choice(seeds))
    mode=i%5
    if mode==0: s=s[:random.randint(0,len(s))]                       # truncated
    elif mode==1:
        for _ in range(random.randint(1,20)): s[random.randrange(len(s))]=random.randrange(256)  # flipped bytes
    elif mode==2:
        p=random.randrange(len(s)); s[p:p]=bytes(random.randrange(256) for _ in range(random.randint(1,50)))
    elif mode==3:
        a=random.randrange(len(s)); del s[a:a+random.randint(1,200)]
    else:
        s=bytearray(b'\xff\xd8'+bytes(random.randrange(256) for _ in range(random.randint(0,3000))))
    open('fz.jpg','wb').write(s)
    t=time.time()
    r=subprocess.run(['./harness','fz.jpg','88','59','fz.raw','60000'],capture_output=True,text=True,timeout=20)
    if time.time()-t>2: slow+=1
    n+=1
    if r.returncode not in (0,1) or 'ERROR' in r.stderr or 'runtime error' in r.stderr:
        crashes+=1
        if crashes<5: open(f'crash{crashes}.jpg','wb').write(s); print('CRASH', r.returncode, r.stderr[-600:])
print(n,'runs',crashes,'crashes',slow,'slow', round(time.time()-t0),'s')
raise SystemExit(1 if crashes else 0)
