#include "net_tsinghua.h"

#include <stdio.h>
#include <unistd.h> // getpass

const char *net_tsinghua_request(const char *url, const char *cookies)
{
  printf("[GET] %s\n", url);
  if (cookies != NULL) printf("Cookie: %s\n", cookies);

#if 1
  // HTTP_PROXY= HTTPS_PROXY= curl `pbpaste` | tr -d '\r' | tr -d '\n' | pbcopy
  static char s[2048];
  gets(s);
  return s;

#else
  if (strcmp(url, "http://3.3.3.3/") == 0) {
    return "<html><head><script type=\"text/javascript\">location.href=\"http://auth4.tsinghua.edu.cn/index_35.html\"</script></head><body>Authentication is required. Click <a href=\"http://auth4.tsinghua.edu.cn/index_35.html\">here</a> to open the authentication page.</body></html>";
  } else if (strcmp(url, "https://auth4.tsinghua.edu.cn/cgi-bin/get_challenge?callback=f&username=user&ip=&double_stack=1") == 0) {
    return "Set-Cookie: thuwebcookie-47873=GCAEAGGFFAAA; Path=/; HttpOnly\n\nf({\"challenge\":\"316f5ecb6c05123c8e25d2a9745e9e16fa0f46760c5801c55a3759cc81f315fa\",\"client_ip\":\"183.173.63.70\",\"ecode\":0,\"error\":\"ok\",\"error_msg\":\"\",\"expire\":\"43\",\"online_ip\":\"183.173.63.70\",\"res\":\"ok\",\"srun_ver\":\"SRunCGIAuthIntfSvr V1.18 B20190423\",\"st\":1732449832})";
  } else {
    return "";
  }
#endif
}

int main()
{
#if 0
  const char *user = "user";
  const char *pwd = "qwqwq";
#else
  char token[128], user[128], *pwd;
  printf("user: "); scanf("%s", user);
  pwd = getpass("pwd: ");
  getchar();
#endif
  int result = net_tsinghua_perform_login(user, pwd);
  printf("%d\n", result);

  return 0;
}
