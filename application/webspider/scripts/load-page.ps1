
param ($start_date, $end_date, $page)

$url="http://www.hshfy.sh.cn/shfy/web/ktgg_search_content.jsp";
$post_data = "yzm=akSF&ft=&ktrqks=$start_date&ktrqjs=$end_date&spc=&yg=&bg=&ah=&pagesnum=$page&ktlx=";
write-debug "url=$post_data";

$req_param = @{
    Accept="*/*";
    'Accept-Encoding'="gzip, deflate";
    'Accept-Language'="ja,en;q=0.9,en-GB;q=0.8,en-US;q=0.7,zh-CN;q=0.6,zh;q=0.5";
    'Content-Length'=$post_data.length;
    'Content-Type'="application/x-www-form-urlencoded";
    DNT=1;
    Host="www.hshfy.sh.cn";
    Origin="http://www.hshfy.sh.cn";
    Referer="http://www.hshfy.sh.cn/shfy/web/ktgg_search.jsp?zd=splc";
    'User-Agent'="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.101 Safari/537.36 Edg/91.0.864.48";
    'X-Requested-With'="XMLHttpRequest";
};

$res = Invoke-WebRequest -uri $url -Headers $req_param -Body $post_data -Method Post
#for debug
$fn = [System.String]::Format(".\temp\page-{0:D4}.html", $page);
$res.Content > $fn;
.\parse-table.ps1 -content $($res.content)
