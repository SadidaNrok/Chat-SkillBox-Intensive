#include <iostream>
#include <string>
#include <uwebsockets/App.h>
#include <thread>
#include <algorithm>
#include <regex>
#include <unordered_map>

using namespace std;

string to_lower(string input) {
    transform(input.begin(), input.end(), input.begin(), ::tolower);
    return input;
}

void bot(uWS::WebSocket<0,1>* ws, uWS::OpCode opCode, unsigned long user, string text) {
    ws->publish("user#" + to_string(user), "[BOT]: " + text, opCode, false);
}

struct UserConnection {
    unsigned long user_id; // очень большие беззнаковые числа
    string user_name;
};

unordered_map<string, string> database = {
        {"hello", "Oh, hello hooman!"},
        {"how are you", "I'm good wby?"},
        {"what is your name", "My name is SkillBot2000"},
        {"what are you doing", "Answering some stupid questions"}
};

int main()
{
    // ws://127.0.0.1:9999/

    atomic_ulong latest_user_id = 2;
    atomic_ulong connected_users = 0;
    vector<thread*> threads(thread::hardware_concurrency());

    transform(threads.begin(), threads.end(), threads.begin(), [&latest_user_id, &connected_users](thread* thr) {
        return new thread([&latest_user_id, &connected_users]() {
            // Что делает данный поток
            uWS::App().ws<UserConnection>("/*", {
                // Что делать серверу в разных ситуациях
                .open = [&latest_user_id, &connected_users](auto* ws) {
                    // Код, который выполняется при новом подключении
                    UserConnection* data = (UserConnection*)ws->getUserData();
                    data->user_id = latest_user_id++;
                    cout << "New user connected, id = " << data->user_id << endl;
                    ws->subscribe("broadcast");
                    ws->subscribe("user#" + to_string(data->user_id));
                    ++connected_users;
                    cout << "Total users connected: " << connected_users << endl;
                },
                .message = [&latest_user_id](auto* ws, string_view message, uWS::OpCode opCode) {
                    // Код, который выполняется при получении нового сообщения
                    UserConnection* data = (UserConnection*)ws->getUserData();
                    // SET_NAME=Mike
                    string_view beginning = message.substr(0,9);
                    if (beginning.compare("SET_NAME=") == 0) {
                        // Пользователь чата представляется
                        string_view name = message.substr(9);
                        if (name.find(",") == -1 && name.size() < 256) {
                            data->user_name = string(name);
                            cout << "User set their name ( ID= " << data->user_id << " ), name = " << data->user_name << endl;
                            // NEW_USER,Mike,10
                            // Уведомляем остальных пользователей чата о новом участнике
                            string broadcast_message = "NEW_USER," + data->user_name + "," + to_string(data->user_id);
                            ws->publish("broadcast", string_view(broadcast_message), opCode, false);
                        }
                        else {
                            //Неверное имя пользователя
                            ws->publish("user#" + to_string(data->user_id),
                                "Your name cannot be longer than 255 symbols and includes no commas.",
                                    opCode, false);
                            cout << "USER (ID = " << data->user_id << ") enter a wrong name." << endl;
                        }
                    }
                    // MESSAGE_TO,11,Hello how are you???
                    string_view is_message_to = message.substr(0, 11);
                    if (is_message_to.compare("MESSAGE_TO,") == 0) {
                        string_view rest = message.substr(11); // id,message
                        int comma_position = rest.find(",");
                        string_view id_string = rest.substr(0, comma_position); // id
                        string_view user_message = rest.substr(comma_position + 1); // message
                        auto i_id = stoi(string(id_string));
                        if (i_id == data->user_id) {
                            //Отправка сообщения самому себе
                            ws->publish("user#" + string(id_string), 
                                "You cannot send messages to yourself.", opCode, false);
                        }
                        else if (i_id < latest_user_id && i_id > 1) {
                            ws->publish("user#" + string(id_string), user_message, opCode, false);
                            cout << "New private message " << message << endl;
                        }
                        else if (i_id == 1) {
                            //Отвечает БОТ
                            int found_phrases = 0;
                            string question = to_lower(string(user_message));
                            for (auto entry : database) {
                                auto pattern = regex(".*" + entry.first + ".*");
                                if (regex_match(question, pattern)) {
                                    bot(ws, opCode, data->user_id, entry.second);
                                    found_phrases++;
                                }
                            }
                            //БОТ не нашёлся, что ответить
                            if (found_phrases == 0) {
                                bot(ws, opCode, data->user_id, "I don't comprende, non ferschtein");
                            }
                        }
                        else {
                            //Пользователя с таким ID не найдено
                            ws->publish("user#" + to_string(data->user_id), 
                                "Error, there is no user with ID = " + string(id_string),
                                    opCode, false);
                        }
                    }
                }

                })
                .listen(9999, [](auto* token) {
                    if (token) {
                        cout << "Server successfully started on port 9999\n";
                    }
                    else {
                        cout << "Something went wrong\n";
                    }
                    })
                    .run();
            });
        });

    for_each(threads.begin(), threads.end(), [](thread* thr) {
        thr->join();
        });
}
