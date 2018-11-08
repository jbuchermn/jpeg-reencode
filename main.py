import time
from build.jpeg_reencode import reencode

data = None
with open('./reference/3_720p.jpg', 'rb') as f:
    data = f.read()

print("Loaded jpeg of size %dkB" % (len(data)/1000))
result_size = 0
t = time.time()
for i in range(100):
    result = reencode(data, 10)
    result_size = len(result)

t = time.time() - t
print("Compressed to jpeg of size %dkB" % (len(result)/1000))
print("FPS: %d => %dMbps" % (100./t, 100. * len(data) * 8. / t / 1.e6))
