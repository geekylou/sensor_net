import uuid

file = open("serial_no.bin","wb")
file.write(uuid.uuid4().bytes)
file.close()