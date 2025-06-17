import sys
from bs4 import BeautifulSoup as Soup

soup = Soup(open("client_fr.ts"),"xml")

contexts = soup.find_all("context")

for context in contexts:
        messages = context.find_all("message")
        for message in messages:
           translation = message.translation.get_text()
           print(translation)
           if not translation:
                source = message.source.get_text()
                print(source)