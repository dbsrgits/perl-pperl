// pperl.h

#define perl_header "use IO::Socket;\n"\
"use strict;\n"\
"use Symbol;\n"\
"use Socket;\n"\
"use POSIX qw(SIGINT SIG_BLOCK SIG_UNBLOCK);\n"\
"use PPerl;\n"\
"\n"\
"# dissociate process...\n"\
"# warn \"forking off now\\n\";\n"\
"defined($PPERL::pid = fork) or die \"Cannot fork: $!\";\n"\
"if ($PPERL::pid) {\n"\
"  CORE::exit(0);\n"\
"}\n"\
"\n"\
"close(STDIN); close(STDOUT); close(STDERR);\n"\
"\n"\
"POSIX::setsid() or die \"Can't start a new session: $!\";\n"\
"\n"\
"#warn \"process started\\n\";\n"\
"\n"\
"$PPERL::SOCKET_NAME = $ARGV[0];\n"\
"$PPERL::PREFORK = $ARGV[1];\n"\
"$PPERL::MAX_CLIENTS_PER_CHILD = $ARGV[2];\n"\
"\n"\
"$PPERL::children = 0;\n"\
"%PPERL::children = ();\n"\
"\n"\
"sub REAPER {\n"\
"    $SIG{CHLD} = \\&REAPER;\n"\
"    my $pid = wait;\n"\
"    $PPERL::children--;\n"\
"    delete $PPERL::children{$pid};\n"\
"}\n"\
"\n"\
"sub HUNTSMAN {\n"\
"    local($SIG{CHLD}) = 'IGNORE';\n"\
"    kill 'INT' => keys %PPERL::children;\n"\
"    CORE::exit(0);\n"\
"}\n"\
"\n"\
"unlink $PPERL::SOCKET_NAME;\n"\
"\n"\
"$PPERL::server = IO::Socket::UNIX->new(\n"\
        "Type => SOCK_STREAM,\n"\
        "Local => $PPERL::SOCKET_NAME,\n"\
        "Listen => SOMAXCONN,\n"\
        "Reuse => 1,\n"\
        ")\n"\
    "or die \"Cannot create socket: $!\";\n"\
"\n"\
"chmod(0777, $PPERL::SOCKET_NAME);\n"\
"# turn on lingering...???\n"\
"setsockopt($PPERL::server, SOL_SOCKET, SO_LINGER, pack(\"II\", 1, 20));\n"\
"\n"\
"$PPERL::reload_modules = 0;\n"\
"$SIG{HUP} = sub { $PPERL::reload_modules++ };\n"\
"\n"\
"END {\n"\
  "close($PPERL::server) if $PPERL::server;\n"\
"}\n"\
"\n"\
"        while ($PPERL::client = $PPERL::server->accept()) {\n"\
"            setsockopt($PPERL::client, SOL_SOCKET, SO_LINGER, pack(\"II\", 1, 20));\n"\
"            open(STDERR, \">>${PPERL::SOCKET_NAME}.err\");\n"\
"\n"\
"            \n"\
            "%ENV = ();\n"\
            "@ARGV = ();\n"\
"            \n"\
"            select($PPERL::client); $|=1;\n"\
"            \n"\
            "PARTS:\n"\
            "while (<$PPERL::client>) {\n"\
                "if (/^\\[DIE\\]$/) {\n"\
                    "REAPER();\n"\
                "}\n"\
                "if (/^\\[ENV\\]$/) {\n"\
                    "while (<$PPERL::client>) {\n"\
                        "redo PARTS if /^\\[/;\n"\
                        "my ($key, $value) = split(/\\s*=\\s*/, $_, 2);\n"\
                        "chomp $value;\n"\
                        "$ENV{$key} = $value;\n"\
                    "}\n"\
                "}\n"\
                "elsif (/^\\[ARGV\\]$/) {\n"\
                    "while (<$PPERL::client>) {\n"\
                        "redo PARTS if /^\\[/;\n"\
                        "chomp;\n"\
                        "push @ARGV, $_;\n"\
                    "}\n"\
                "}\n"\
                "elsif (/^\\[STDIO\\]$/) {\n"\
                    "#warn \"binding STDIO\\n\";\n"\
                    "open(STDIN, \"<&\" . $PPERL::client->fileno) || die \"can't dup CLIENT: $!\";\n"\
                    "open(STDOUT, \">&\" . $PPERL::client->fileno) || die \"can't dup CLIENT: $!\";\n"\
                    "last PARTS;\n"\
                "}\n"\
"                \n"\
            "}\n"\
"            \n"\
            "send($PPERL::client, \"OK\\n\", 0);\n"\
            "# program here\n"\
            "eval {\n"\
"\n"

#define perl_footer ""\
"            };\n"\
"            if ($@) {\n"\
"                if ($@ !~ /PPERLexit(\\d+)/) {\n"\
"                    warn $@;\n"\
"                }\n"\
"                else {\n"\
"                    print STDERR \"RETVAL:$1\\n\";\n"\
"                }\n"\
"            }\n"\
"            #warn(\"program finished\\n\");\n"\
"            shutdown($PPERL::client, 2);\n"\
"            undef($PPERL::client);\n"\
"            close(STDERR);\n"\
"            if ($PPERL::reload_modules) {\n"\
"                CORE::exit(0);\n"\
"            }\n"\
"        }\n"\
"\n"
