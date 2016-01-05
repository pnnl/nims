// Test saving the M3 TCP/IP data packets to a binary file for playback
// with M3 software.



int main(int argc, char *argv[])
{
    // connect to host
    struct sockaddr_in m3_host;
    
    m3_host.sin_family = AF_INET;
    m3_host.sin_addr.s_addr = inet_addr("130.20.41.225");
    m3_host.sin_port = htons(20001); // TODO: maybe make this an arg
    
    m3 = -1;
    m3 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( connect(input_, (struct sockaddr *) &m3_host, sizeof(struct sockaddr)) < 0 )
    {
        perror("Error connecting to M3 host:");
        close (m3);
        return -1;
    }

    // get sync words for start of packet
    int count = 0;
    while (count < 50)
    {
        ++count;
        clog << count << endl;
    }

}
