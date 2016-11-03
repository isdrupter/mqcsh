<pre>
Program start -- |


          main_loop -- | - get_config()
                          - determine shell (bash, ash, sh)
                          - determine pub ip address (this will be ident)
                          - determine where to put temp files (/tmp/.mqsh)
                         
                       | - mosquitto_run(host, user, password, ident, subscribe topic(s), publish topic(s), shell,)
                              | - mosquitto connect ( args )
                                | - receive command 
                                    - pass to base64(  command, decode )
                                      - get output from base64 -
                                      - determine what to do with decoded cmd: 
                                        - it is this cmd prefixed with _SH_ ?
                                           (  or _GET_, _UPDATE_, or some other flag ? - worry  about his later )
                                          -then  pass to execute(json=no) 
                                        - otherwise
                                          - pass to function execute(json=yes)
                       |--base64(input, mode)
                          - is mode decode? decode it, return decoded output
                          - is mode encode? encode it, return necoded output
                       |-- execute (cmd, jsonify) [start a new thread here]
                       |  -- receives a command from mosquitto 
                            - executes in a deamonized thread with whatever $shell it is using (bash, sh, ash, ksh, etc)
                            - is this an _SH_ command ?
                              - if yes  pass output directly to publish
                              - if no, pass output to jsonify
                       |--jsonify(input)
                          -- formats shell output to json
                          -- pass output to publish()
                       |-- publish(output, topics)
                         - base64 encode (input, encode)
                          - mosquitto_pub (base64_output)
                           - was that successful? 
                              - if so , we're done here
                              - if not, maybe usleep $(RANDOM microseconds) and try again?
                              
</pre>
