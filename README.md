# PM3.0
A simple web based Password Manager utilizing a C++ Backend, JQuery, HTML5 and W3CSS for its frontend

# Outline
Utilizing the advancements of SMPL (A C++ library I have created), PM3.0 is a simple to use and understand version of my older closed source password managers.

It uses a web based interface that allows it to be naturally cross platform and also allows easy synchronization between devices ensuring the data is always up to date. Backing up data is as simple as copying the user_info folder generated to a different location.

# Usage
To set up the Password Manager, simply generate your own SSL certificate and key (or use a non self signed one if you have the funds) and provide the files to the config.ini file it generates on first launch if you don't have the config.ini file already. 

It is possible to run without SSL meaning you will not have HTTPS. It is not as safe as you will be sending session keys back and forth which could be swiped if you happen to be using this on unsecure wifi or if your computer has been compromised and some software is listening in on all your out going packets.

If you desire to use the password manager from outside the local area network it exist in, simply setup port forwarding on the desired port you selected in config.ini and set the ip address to your external ip.

This password manager also allows the use of raw API calls using whatever application you wish as long as it can send HTTP messages and receive them. This can allow you to create your own interface if desired.

# Web Interface
The home page looks like this when not logged in:
![alt text](https://github.com/Davidah121/PM3.0/blob/master/images/home.png?raw=true)

From here, you can login by using the side panel. You are unable to add new entries as you aren't logged in and it wouldn't be able to save that data.

The login page is simple as well:
![alt text](https://github.com/Davidah121/PM3.0/blob/master/images/login_page.png?raw=true)

Here, you can login or create a new account if need be. You may have as many accounts as you wish however, password recovery may not be possible without external password cracking tools. Please make sure you can remember your password.


The Home page will now populate whatever entries you have for that account and will be in the order you added them:
![alt text](https://github.com/Davidah121/PM3.0/blob/master/images/home_add_data.png?raw=true)

You may now use the add entry button (the green plus) and when you are done, press the green check to add it.

Editing entries works the same way but you can not edit the time and date values. If you press the trash button it will delete that entry.

There is no Undo, Redo, or "Are you sure?" features so make sure you are careful.

The Settings page exists only if you are logged in:
![alt text](https://github.com/Davidah121/PM3.0/blob/master/images/settings.png?raw=true)

Allows you to change your password, delete your account, or just log out though you can log out from the side bar anytime as well.

There is no "Are you sure?" feature so make sure you are careful where you tap or press.

# API Interface
It is possible to interface with the password manager entirely using curl or any other tool that allows you to send HTTP messages.

These are the allowed GET Requests:
- /api/list_all_entries
  - Returns all entries for the user as a JSON
  - Requires a valid session cookie passed along
- /api/get_user_info
  - Returns the information about the user as a JSON
  - Requires a valid session cookie passed along

```JSON
{
  [
    {
      Name: "Entry1"
    },
    {
      Name: "Entry2"
    }
  ]
}
```
These are the allowed POST Requests:
- /api/login
  - Attempts to login using the given username and password provided in JSON format
  - On success, provides a session key as a cookie
- /api/create_account
  - Attempts to create an account using the given username and password provided in JSON format
  - Same format as the login JSON format
  - On success, provides a session key as a cookie
- /api/logout
  - Attempts to logout using the session key passed as a cookie
- /api/delete_account
  - Attempts to delete an account using the session key passed as a cookie
- /api/add_entry
  - Attempts to add a new entry using the session key passed as a cookie and a JSON of the entry fields
- /api/delete_entry
  - Attempts to delete an entry using the session key passed as a cookie and a JSON of the entry fields.
  - Only the name of the entry is needed
- /api/import_entries
  - Attempts to import a JSON of entries using the session key passed as a cookie
  - Must be formatted in the same way add_entry expects a single entry to be formatted
- /api/change_password
  - Attempts to change the password of the account using the session key passed as a cookie and the passwords passed in JSON format

