import requests

apiToken = '6789017033:AAENa3GfTLng5-k0jsLDF_ZJAtWQ6cJr1jE'
chatID = -4150974588

def send_to_telegram(message):  # Telegram bot
    apiURL = f'https://api.telegram.org/bot{apiToken}/sendMessage'
    try:
        response = requests.post(apiURL, json={'chat_id': chatID, 'text': message})
        print(response.text)
    except Exception as e:
        print(e)

#send_to_telegram("test")