#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>  // Dla funkcji system
#include <string.h> // stringi zawsze spoko

#include <sstream> 

// Kolory tekstowe w sekwencjach ucieczki ANSI
#define GREEN_TEXT "\033[1;32m"
#define RED_TEXT "\033[1;31m"
#define RESET_TEXT "\033[0m"

using namespace std;

string githublogin;
string githubmail;

int coutGreen(string s){
	cout << GREEN_TEXT << s << RESET_TEXT;
	return 0;
}

int coutRed(string s){
	cout << RED_TEXT << s << RESET_TEXT;
	return 0;
}


// Pobierz nową wersję z serwera
bool checkAndUpdateVersion() {
        system("sudo rm /bin/studentmachine*");
	system("sudo wget https://github.com/marcin-filipiak/cpp_studentmachine/raw/main/client/build/studentmachine -P /bin");
	system("sudo chmod +x /bin/studentmachine");
	coutGreen("\n App StudentMachine was updated\n");    
	return true;
}

//zapis do pliku
int savetofile(string data, string f){
    std::ofstream plik(f);
    if (plik.is_open()) {
        // Zapisz tekst do pliku
        plik << data;
    }
     // Zamknij plik
     plik.close();
    return 0;
}

//funkcja do wywolywania komend systemowych i przechwytywania ich wyniku
std::string executeCommand(const char* command) {
    const int bufferSize = 128;
    char buffer[bufferSize];
    std::string result = "";

    FILE* pipe = popen(command, "r");
    if (!pipe) {
        coutRed("\n Something gone wrong with command :-/\n");
        return "";
    }

    while (fgets(buffer, bufferSize, pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);

    // Usun ewentualny znak nowej linii z wyniku
    result.erase(result.find_last_not_of("\n") + 1);

    return result;
}

//czy agent ssh dziala i ma pid
bool isSSHPIDSet() {
    const char* sshPID = std::getenv("SSH_AGENT_PID");
    return (sshPID != nullptr && strlen(sshPID) > 0);
}

//zapis konfiguracji do plikow
int saveconfig(){
    const char* homeDir = getenv("HOME");

    std::ofstream outputFile(std::string(homeDir) + "/.studentmachine/githublogin");
    outputFile << githublogin;
    outputFile.close();

    std::ofstream outputFile2(std::string(homeDir) + "/.studentmachine/githubmail");
    outputFile2 << githubmail;
    outputFile2.close();
    return 0;
}

//wczytanie konfiguracji z plikow
int loadconfig(){
    const char* homeDir = getenv("HOME");

    std::ifstream file(std::string(homeDir) + "/.studentmachine/githublogin");
    std::getline(file, githublogin);
    file.close();

    std::ifstream file2(std::string(homeDir) + "/.studentmachine/githubmail");
    std::getline(file2, githubmail);
    file2.close();

    return 0;
}

//konfiguruj gita
int gitconfig(){
        //konfig gita
        string s = "git config --global user.email \""+githubmail+"\"";
        system(s.c_str());
        s = "git config --global user.name \""+githublogin+"\"";
        system(s.c_str());
        return 0;
}

//rejestracja klucza w systemie
int keyregister(){
        //ustawienie praw do klucza prywatnego
        string s = "chmod 600 ~/.ssh/"+githublogin;
        system(s.c_str());

        if (isSSHPIDSet() == false){
            cout << "\n Iam starting ssh agent and setting up pid and sock\n";

            //uruchomienie agenta ssh
            s = "eval \"$(ssh-agent -s)\"";
            system(s.c_str());

            /*
            //zapisanie w zmiennych srodowiskowych pid agenta i sock
            s = "export SSH_AGENT_PID=$(pgrep ssh-agent)";
            system(s.c_str());

            s = "export SSH_AUTH_SOCK=$(find /tmp/ssh-* -type s | head -n 1)";
            system(s.c_str());
            */

           string buffer = executeCommand("pgrep ssh-agent");
           setenv("SSH_AGENT_PID", buffer.c_str(), 1);

           string buffer2 = executeCommand("find /tmp/ssh-* -type s | head -n 1");
           setenv("SSH_AUTH_SOCK", buffer2.c_str(), 1);

        }
        else {
            cout<<"\n SSH agent is ok\n";
        }


        //dodanie kluczy do ssh
        s = "ssh-add ~/.ssh/"+githublogin;
        system(s.c_str());

        return 0;
}


//inicjacja przestrzeni roboczej zgodnie z szablonem
int initworkspace(){
    cout << "\n Iam downloading templates\n";
    //pobranie szablonow i instrukcji
    system("cd ~ && wget -nc http://api.noweenergie.org/application/StudentMachine/files/student_projects.tar");
    system("tar -xvf ~/student_projects.tar");
    
    cout << "\n Now script to configure workspace\n";

    system("chmod +x ~/student_projects/run.sh");
    //uruchomienie skryptu konfigurujacego
    system("sh ~/student_projects/run.sh");

    cout << "\n Cleaning after work...\n";
    //wyczyszczenie plikow roboczych
    system("rm ~/student_projects/run.sh");
    system("rm ~/student_projects.tar");

    cout << "\n And linking workspace for Apache lovers\n";
    //podlinkowanie folderu apache
    system("sudo chmod 777 -R /var/www/html");
    system("sudo ln -s /var/www/html ~/student_projects/html");
    return 0;
}

///////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{

    cout << "\n\n STUDENT MACHINE";
    cout << "\n version: big bubble";
    cout << "\n by m.filipiak\n\n";
    if (argc == 1) {
            cout << "\nI need parameters to run.\n";
            cout << "Try with:\n";
            cout << " install\n";
            cout << " systemup\n";
            cout << " savework\n";
            cout << " systemdown\n";
            cout << " update\n";
            cout << " templates\n";
            cout << "\n\n";
            return 0;
    }

    else {

        string parm = argv[1];

        //------------first start and system setup
        if (parm == "install"){
            system("sudo apt install -y g++ nano vim git apache2 mariadb-server mariadb-client phpmyadmin");
	    checkAndUpdateVersion();
	    //TODO: konfiguracja sciezki apache i mysql do folderu student_projects
        }

        //------------system update
        if (parm == "update"){
	    system("cd ~ && mkdir .studentmachine");
            checkAndUpdateVersion();
	}

        //------------system start and login
        if (parm == "systemup"){
            //wyczyszczenie projektow ucznia
            system("rm -rf ~/student_projects");

            //czyszczenie historii systemu
            system("rm ~/.bash_history");

	   //czyszczenie ustawien usera
	   system("rm -r ~/.studentmachine");
	   system("cd ~ && mkdir .studentmachine");

            
            coutGreen("What about github? [u]se / [t]hanks / [r]egister\n");
            char menu;
            cin >> menu;
            //mamy i bedziemy uzywac gita
            if (menu == 'u'){
                
                cout << "Github login:\n";
                cin >> githublogin;

                cout << "Github email:\n";
                cin >> githubmail;

                //pobranie klucza prywatnego (-nc by nie nadpisywal jak jest)
                string s = "cd ~/.ssh && wget -nc http://api.noweenergie.org/application/StudentMachine/keyring/"+githublogin; 
                system(s.c_str());
                //pobranie klucza publicznego (-nc by nie nadpisywal jak jest)
                s = "cd ~/.ssh && wget -nc http://api.noweenergie.org/application/StudentMachine/keyring/"+githublogin+".pub";
                system(s.c_str());

                //rejestruj klucz w systemie
                keyregister();

                //konfiguruj gita
                gitconfig();

                //zapis do pliku konfiga
                saveconfig();

                //pobranie repo
                s = "cd ~ && git clone git@github.com:"+githublogin+"/student_projects.git";
                system(s.c_str());
            }

                //pierwszy raz do githuba z tego systemu - generujemy więc klucze
            if (menu == 'r') {
                cout << "Github login:\n";
                cin >> githublogin;
                cout << "Github email:\n";
                cin >> githubmail;
                
                string s;

                //generuj klucz
                s = "ssh-keygen -t ed25519 -C \""+githubmail+"\" -f ~/.ssh/"+githublogin;
                system(s.c_str());

                //poslanie klucza prywatnego i publicznego do keyringa
                cout << "\n Iam sending keys to keyring";

                s = "cd ~/.ssh && curl -X POST -F \"data=@"+githublogin+"\" \"http://api.noweenergie.org/application/StudentMachine/index.php\"";
                system(s.c_str());
                s = "cd ~/.ssh && curl -X POST -F \"data=@"+githublogin+".pub\" \"http://api.noweenergie.org/application/StudentMachine/index.php\"";
                system(s.c_str());

                cout << "\n Registering keys in Linux";
                //rejestruj klucz w systemie
                keyregister();

                cout << "\n Git client configuration";
                //konfiguruj gita
                gitconfig();

                cout << "\n Config files saving";
                //zapis konfiga
                saveconfig();
                
                coutRed("\n\n Now your move, config everything on github.com:");
                cout << "\n - make private repo: \"student_projects\"";
                cout << "\n - add the key to git from:  http://api.noweenergie.org/application/StudentMachine/keyring/";
                cout << "\n while everything is ready say [y]es \n";
                char r;
                cin >> r;
                if (r=='y'){
                    //pobranie repo
                    s = "cd ~ && git clone git@github.com:"+githublogin+"/student_projects.git";
                    system(s.c_str());

                    initworkspace();
                }
            }

            
            //bez gita
            if (menu == 't'){
                
                initworkspace();

                coutGreen("\nOk done, but I dont save your work...\n");
            }
        }

        //------------aktualizacja temlejtek
        if (parm == "templates"){
		initworkspace();
	}

        //------------zapis pracy do repo
        if (parm == "savework"){
            //wczytanie loginu 
            loadconfig();

            if (githublogin != ""){
                string buffer = executeCommand("pgrep ssh-agent");
                setenv("SSH_AGENT_PID", buffer.c_str(), 1);

                string buffer2 = executeCommand("find /tmp/ssh-* -type s | head -n 1");
                setenv("SSH_AUTH_SOCK", buffer2.c_str(), 1);

                //wyslanie pracy do repozytorium
                const char* command = "cd ~/student_projects && git add . && git commit -m \"studentmachine send\" && git push";
                int result = system(command);
            }
            else {
                coutRed("Whops! I havent your github login... :-O\n");
            }
        }

        //------------system is shutting down
        if (parm == "systemdown"){
            
            //wczytanie loginu 
            loadconfig();

            //jesli mamy login do githuba
            if (githublogin !=""){

                string buffer = executeCommand("pgrep ssh-agent");
                setenv("SSH_AGENT_PID", buffer.c_str(), 1);

                string buffer2 = executeCommand("find /tmp/ssh-* -type s | head -n 1");
                setenv("SSH_AUTH_SOCK", buffer2.c_str(), 1);

                //wyslanie pracy do repozytorium
                const char* command = "cd ~/student_projects && git add . && git commit -m \"wyslanie pracy\" && git push";
                int result = system(command);

                //todo: jak nic nie robil to nie robi sie push i wyskakuje whops

                // Sprawdzenie wyniku
                if (result == 0) {
                    coutGreen("\n Iam saving your work on github - Bye! :-) \n");
                    
                    string s;
                    //wyczyszczenie kluczy
                    s = "rm ~/.ssh/"+githublogin;
                    system(s.c_str());
                    s = "rm ~/.ssh/"+githublogin+".pub";
                    system(s.c_str());

                    //wylaczenie maszyny
                    system("sudo poweroff");
                }
                else {
                    coutGreen("\n Now nothing to post on github - Bye! \n");
                    //wylaczenie maszyny
                    system("sudo poweroff");
                }
            }
            else {
                coutGreen("Bye! But this time with no github. I am sure that you saved own work.\n");
            }
        }

    }

    return 0;

}
