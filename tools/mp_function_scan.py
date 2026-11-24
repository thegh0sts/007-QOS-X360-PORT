import struct, json, re, glob, sys
# Usage: mp_function_scan.py DECRYPTED_IMAGE GENERATED_DIR OUT.txt (image: default_mp.xex decrypted, loaded at
# 0x82000000; GENERATED_DIR: mp/generated/default after a codegen run). Writes 'ADDRESS [data]' lines.
img=open(sys.argv[1],'rb').read()
B=0x82000000; CODE_LO=0x820E0000; CODE_HI=0x82535754
def w(a): return struct.unpack('>I',img[a-B:a-B+4])[0]
gen=sys.argv[2]
part=json.load(open(gen+'/codegen.partition.json'))['assignments']
F=set(int(k,16) for k in part)
L=set(); 
for f in glob.glob(gen+'/qosmp_recomp.*.cpp'):
    s=open(f).read()
    L.update(int(m,16) for m in re.findall(r'^loc_([0-9A-F]+):',s,re.M))
    F.update(int(m,16) for m in re.findall(r'^DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)',s,re.M))
# import stubs: li/lis r11 ... lwz r11; mtctr r11; bctr pattern near end: find first stub
stub_lo=0x8252516C
for a in []:
    if w(a)>>16==0x3d60 and (w(a+4)>>16)==0x816b and w(a+8)==0x7d6903a6 and w(a+12)==0x4e800420:
        stub_lo=a; break
print('import stubs from',hex(stub_lo) if stub_lo else None, file=sys.stderr)
# data words pointing into code
datarefs=set()
for off in range(0,len(img)-3,4):
    a=B+off
    if CODE_LO<=a<CODE_HI: continue
    v=struct.unpack('>I',img[off:off+4])[0]
    if CODE_LO<=v<CODE_HI and v%4==0: datarefs.add(v)
out=[]
for a in range(CODE_LO+4,CODE_HI,4):
    if stub_lo and a>=stub_lo-0x40: break
    p=w(a-4); x=w(a)
    uncond = p in (0x4e800020,0x4e800420) or ((p>>26)==18 and (p&3)==0)
    if not uncond: continue
    if a in F or a in L: continue
    if x==0 or (CODE_LO<=x<0x83000000 and (x>>24)==0x82): continue  # padding / inline table
    if (x>>26)==0: continue
    out.append(a)
print(len(out), sum(1 for a in out if a in datarefs), file=sys.stderr)
with open(sys.argv[3],'w') as f:
    for a in out: f.write('%08X %s\n'%(a,'data' if a in datarefs else ''))
