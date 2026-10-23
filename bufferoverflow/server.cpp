#include "httplib.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

struct User {
    char username[128];
    char password[32];
    char privateinfo[256];
};

std::vector<User> users;

std::string html_escape(const std::string& s) {
    std::string out;

    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }

    return out;
}

std::string login_page(const std::string& error = "") {
    std::string page = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Demo Login</title>
</head>
<body style="font-family:Arial,sans-serif;max-width:420px;margin:80px auto;padding:20px;">
    <h1>Login</h1>
)";

    if (!error.empty()) {
        page += "<p style=\"color:red;\">" + html_escape(error) + "</p>";
    }

    page += R"(
    <form method="POST" action="/login">
        <label>Username</label><br>
        <input type="text" name="username" required
               style="width:100%;box-sizing:border-box;padding:8px;margin:6px 0 14px;"><br>

        <label>Password</label><br>
        <input type="password" name="password" required
               style="width:100%;box-sizing:border-box;padding:8px;margin:6px 0 14px;"><br>

        <button type="submit" style="padding:9px 18px;">Login</button>
    </form>

    <p><a href="/register">Create an account</a></p>
</body>
</html>
)";

    return page;
}

std::string register_page(const std::string& error = "") {
    std::string page = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Register</title>
</head>
<body style="font-family:Arial,sans-serif;max-width:420px;margin:80px auto;padding:20px;">
    <h1>Register</h1>
)";

    if (!error.empty()) {
        page += "<p style=\"color:red;\">" + html_escape(error) + "</p>";
    }

    page += R"(
    <form method="POST" action="/register">
        <label>Username</label><br>
        <input type="text" name="username" required
               style="width:100%;box-sizing:border-box;padding:8px;margin:6px 0 14px;"><br>

        <label>Password</label><br>
        <input type="password" name="password" required
               style="width:100%;box-sizing:border-box;padding:8px;margin:6px 0 14px;"><br>
               
        <label>Private Info</label><br>
        <input type="textarea" name="private" required
               style="width:100%;box-sizing:border-box;padding:8px;margin:6px 0 14px;"><br>

        <button type="submit" style="padding:9px 18px;">Register</button>
    </form>

    <p><a href="/">Back to login</a></p>

    <p style="font-size:13px;color:#666;">
        Username limit: 127 characters<br>
        Password limit: 31 characters<br>
        Private Info limit: 255 characters
    </p>
</body>
</html>
)";

    return page;
}

std::string private_page(const User& user) {
    std::string page = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Private Area</title>
</head>
<body style="font-family:Arial,sans-serif;max-width:600px;margin:80px auto;padding:20px;">
)";

    page += "<h1>Welcome, " + html_escape(user.username) + "</h1>";

    page += R"(
    <p>You successfully authenticated.</p>

    <div style="padding:15px;border:1px solid #ccc;margin-top:20px;">
        <strong>Private information</strong>
        <p>)";

	page += "<form method=\"POST\" action=\"/updateinfo\">";
	page += "<textarea name=\"privateinfo\" rows=\"8\" style=\"width:100%;box-sizing:border-box;margin-top:10px;padding:8px;\">";
    page += html_escape(user.privateinfo);
    page += "</textarea>";
    page += "<input type=\"hidden\" name=\"username\" required value=\"" + html_escape(user.username) + "\">";
    page += "<input type=\"hidden\" name=\"password\" required value=\"" + html_escape(user.password) + "\">";
    page += R"(
		<button type="submit" style="margin-top:10px;padding:9px 18px;" > Save </button> </form>
	)";

    page += R"(</p>
    </div>

    <p><a href="/">Log out</a></p>
</body>
</html>
)";

    return page;
}

int main() {
    httplib::Server server;

    /*
     * Registration page.
     */
    server.Get("/register", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(register_page(), "text/html");
    });

    /*
     * Registration.
     */
    server.Post("/register", [](const httplib::Request& req,
                                httplib::Response& res) {

        if (!req.has_param("username") || !req.has_param("password") || !req.has_param("private")) {
            res.set_content(register_page("Missing username or password."),
                            "text/html");
            return;
        }

        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");
        std::string privateinfo = req.get_param_value("private");

        /*
         * IMPORTANT:
         * This validation is intentionally based on strlen().
         *
         * strlen() stops at the first '\0'.
         */
        if (std::strlen(username.c_str()) >= sizeof(User::username)) {
            res.set_content(
                register_page("Username is too long."),
                "text/html"
            );
            return;
        }

        if (std::strlen(password.c_str()) >= sizeof(User::password)) {
            res.set_content(
                register_page("Password is too long."),
                "text/html"
            );
            return;
        }
        
        if (std::strlen(privateinfo.c_str()) >= sizeof(User::privateinfo)) {
            res.set_content(
                register_page("Private Info is too long."),
                "text/html"
            );
            return;
        }

        for (const auto& user : users) {
            if (std::strcmp(user.username, username.c_str()) == 0) {
                res.set_content(
                    register_page("Username already exists."),
                    "text/html"
                );
                return;
            }
        }

        User user{};

        /*
         * Normal registration copies.
         */
        std::strncpy(
            user.username,
            username.c_str(),
            sizeof(user.username) - 1
        );

        std::strncpy(
            user.password,
            password.c_str(),
            sizeof(user.password) - 1
        );
        
        std::strncpy(
            user.privateinfo,
            privateinfo.c_str(),
            sizeof(user.privateinfo) - 1
        );

        users.push_back(user);

        res.set_redirect("/", 303);
    });

    /*
     * Login.
     */
    server.Post("/login", [](const httplib::Request& req,
                             httplib::Response& res) {

        if (!req.has_param("username") || !req.has_param("password")) {
            res.set_content(login_page("Missing username or password."),
                            "text/html");
            return;
        }

        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");

        /*
         * Again, length checking is based on strlen().
         */
        if (std::strlen(username.c_str()) >= sizeof(User::username)) {
            res.set_content(
                login_page("Username is too long."),
                "text/html"
            );
            return;
        }

        if (std::strlen(password.c_str()) >= sizeof(User::password)) {
            res.set_content(
                login_page("Password is too long."),
                "text/html"
            );
            return;
        }

        /*
         * Find username first.
         */
        for (const auto& user : users) {

            if (std::strcmp(user.username, username.c_str()) == 0) {

                /*
                 * Then check the password.
                 */
                
                
                // MY VERY EPIC PASSWORD CHECK LOGIC :D
                struct {
                	char passwordcstr[32] = { 0 };
                	unsigned int k = 0;
				} temp;
                
                std::memcpy(
		            temp.passwordcstr,
		            password.c_str(),
		            password.size()
		        );
                
                for (int i = 0; i < 32; i++) {
                	if (user.password[i] != temp.passwordcstr[i])
                		temp.k++;
				}
				
				if (temp.k == 0) {
					res.set_content(private_page(user), "text/html");
					return;
				}

                res.set_content(
                    login_page("Incorrect password."),
                    "text/html"
                );
                return;
            }
        }

        res.set_content(
            login_page("User not found."),
            "text/html"
        );
    });
    
    server.Post("/updateinfo", [](const httplib::Request& req,
                             httplib::Response& res) {

        if (!req.has_param("username") || !req.has_param("password") || !req.has_param("privateinfo")) {
            res.set_content(login_page("Missing username or password."),
                            "text/html");
            return;
        }

        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");
        std::string privateinfo = req.get_param_value("privateinfo");

        /*
         * Again, length checking is based on strlen().
         */
        if (std::strlen(username.c_str()) >= sizeof(User::username)) {
            res.set_content(
                login_page("Username is too long."),
                "text/html"
            );
            return;
        }

        if (std::strlen(password.c_str()) >= sizeof(User::password)) {
            res.set_content(
                login_page("Password is too long."),
                "text/html"
            );
            return;
        }
        
        if (std::strlen(privateinfo.c_str()) >= sizeof(User::privateinfo)) {
            res.set_content(
                login_page("Private Info is too long."),
                "text/html"
            );
            return;
        }

        /*
         * Find username first.
         */
        for (auto& user : users) {

            if (std::strcmp(user.username, username.c_str()) == 0) {

                /*
                 * Then check the password.
                 */
                if (std::strcmp(user.password, password.c_str()) == 0) {
                	std::memcpy(
			            user.privateinfo,
			            privateinfo.c_str(),
			            privateinfo.size()
			        );
                	
                    res.set_content(private_page(user), "text/html");
                    return;
                }

                res.set_content(
                    login_page("Incorrect password."),
                    "text/html"
                );
                return;
            }
        }

        res.set_content(
            login_page("User not found."),
            "text/html"
        );
    });
    
    /*
     * Login page. At the end cause its a pattern, if it is kept at the first then it always matches cause it has max priority 
     *
     * Because this is a demo server, every GET path other than
     * /register will show the login page.
     */
    server.Get(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(login_page(), "text/html");
    });

    /*
     * Listen on localhost:8080.
     */
    std::cout << "Server running at http://127.0.0.1:8080\n";

    if (!server.listen("127.0.0.1", 8080)) {
        std::cerr << "Failed to start server.\n";
        return 1;
    }

    return 0;
}