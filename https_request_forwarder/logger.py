import queue
import threading 
from datetime import datetime
class logger:
    
    def __init__(self):
        self.queue = queue.Queue()
        self.work = True
        self.worker = threading.Thread(target=self.work_func)
        self.worker.start()


    def log(self, data):
        header = f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}|{threading.get_native_id()}|".encode("utf-8")
        self.queue.put(header + data)
    
    def stop(self):
        self.work = False
        self.worker.join()

    def work_func(self):
        with open('log.txt', 'wb') as file:
            while self.work == True:
                try:
                    data = self.queue.get(block=True, timeout=2)
                    if data is None:
                        continue
                    file.write(data)
                    print(data)
                except queue.Empty:
                    continue

            while self.queue.full():
                try:
                    data = self.queue.get()
                    if data is None:
                        continue
                    file.write(data)
                    print(data)
                except queue.Empty:
                    break