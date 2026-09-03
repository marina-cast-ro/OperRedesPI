def storeData(information, path="message.txt"):
    with open(path, "a") as message: # "a" for append
        message.write(information + "\n") #aqui va el mensaje 
