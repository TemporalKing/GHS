#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#include <vector>
#include <map>
#include <queue>
using namespace std;

const int oo=1e9;
const size_t nil=-1;
const int UNKNOWN=-1;

enum PROCESSOR_STATE{
    SLEEPING,
    FIND,
    FOUND
};

const char * getStateDesc(const PROCESSOR_STATE s) {
    switch (s) {
    case SLEEPING:
        return "Sleeping";
    case FIND:
        return "Find";
    case FOUND:
        return "Found";
    default:
        return NULL;
    }
}

enum EDGE_STATE {
    BASIC,
    BRANCH,
    REJECTED,
    ANY
};

enum MESSAGE_TYPE {
    CONNECT,
    INITIATE,
    TEST,
    ACCEPT,
    REJECT,
    REPORT,
    CHANGE_ROOT
};

const char * getMessageDesc(const MESSAGE_TYPE type) {
    switch (type) {
    case CONNECT:
        return "CONNECT";
    case INITIATE:
        return "INITIATE";
    case TEST:
        return "TEST";
    case ACCEPT:
        return "ACCEPT";
    case REJECT:
        return "REJECT";
    case REPORT:
        return "REPORT";
    case CHANGE_ROOT:
        return "CHANGEROOT";
        break;
    default:
        return NULL;
    }
}

struct EDGE {
    size_t e_id;
    int v_dest;
    EDGE_STATE e_state;
    int e_weight;
};

struct MESSAGE {
    MESSAGE_TYPE type;
    int v_src;
    union {
        struct {
            int remoteLN;
        }CONNECTMSG;
        struct {
            int remoteLN;
            int remoteFN;
            PROCESSOR_STATE S;
        }INITIATEMSG;
        struct {
            int remoteLN;
            int remoteFN;
        }TESTMSG;
        struct {
            int candidateW;
        }REPORTMSG;
    };
};

class PROCESSOR {
private:
    PROCESSOR();

    const int m_num;

    vector<EDGE> m_SE;
    PROCESSOR_STATE m_SN;
    int m_FN;
    int m_LN;
    size_t m_bestEdge;
    int m_bestWT;
    size_t m_testEdge;
    size_t m_inBranch;
    int m_findCount;

    deque<MESSAGE> m_msgQueue;

public:
    PROCESSOR(const int num):m_num(num) {
        m_SN=SLEEPING;
    }

    ~PROCESSOR() {
        printf("Vertex %d deconstructor\n",getVertexNumber());
    }

    static bool sendMessage(const MESSAGE_TYPE type,const PROCESSOR * const me,const EDGE& e,MESSAGE& msg);

    int getVertexNumber() const {return m_num;}
    int getLevel() const {return m_LN;}
    int getID() const {return m_FN;}
    PROCESSOR_STATE getState() const {return m_SN;}

    void enterState(const PROCESSOR_STATE s) {
        m_SN=s;
        if (s!=SLEEPING) printf("Vertex %d entered state %s\n",getVertexNumber(),getStateDesc(s));
    }

    void setLevel(const int ln) {
        m_LN=ln;
        printf("Vertex %d has new level %d\n",getVertexNumber(),ln);
    }

    void setID(const int fn) {
        m_FN=fn;
        printf("Vertex %d has new fragment number(core) %d\n",getVertexNumber(),fn);
    }


    size_t vertex2edge(const int remoteVertex) const {
        for (size_t i=0;i<m_SE.size();i++)
            if (m_SE[i].v_dest==remoteVertex)
                return i;
        return nil;
    }

    size_t findMinEdge(const EDGE_STATE state) const {
        size_t ret=nil;
        for (size_t i=0;i<m_SE.size();i++)
            if ((m_SE[i].e_state==state || state==ANY) && (ret==nil || m_SE[i].e_weight<m_SE[ret].e_weight))
                ret=i;
        return ret;
    }

    void pushMessage(const MESSAGE& msg) {
        m_msgQueue.push_back(msg);
    }

    bool processMessage() {
        MESSAGE msg;
        size_t cnt=m_msgQueue.size();
        while (cnt--) {
            msg=m_msgQueue.front();
            m_msgQueue.pop_front();

            const size_t j=vertex2edge(msg.v_src);
            assert(j!=nil);

            bool processed=false;

            printf("Vertex %d is processing message %s from %d ",getVertexNumber(),getMessageDesc(msg.type),msg.v_src);

            switch (msg.type) {
            case CONNECT:
                processed=onConnect(m_SE[j],msg.CONNECTMSG.remoteLN);
                break;
            case INITIATE:
                processed=OnInitiate(m_SE[j],msg.INITIATEMSG.remoteLN,msg.INITIATEMSG.remoteFN,msg.INITIATEMSG.S);
                break;
            case TEST:
                processed=onTest(m_SE[j],msg.TESTMSG.remoteLN,msg.TESTMSG.remoteFN);
                break;
            case ACCEPT:
                processed=onAccept(m_SE[j]);
                break;
            case REJECT:
                processed=onReject(m_SE[j]);
                break;
            case REPORT:
                processed=onReport(m_SE[j],msg.REPORTMSG.candidateW);
                break;
            case CHANGE_ROOT:
                processed=onChangeRoot(m_SE[j]);
                break;
            default:
                break;
            }
            if (!processed) {
                pushMessage(msg);
                puts("[delayed]");
            }else {
                puts("[DONE!]");
                return processed;
            }
        }
        return false;
    }

    void addEdge(const int remoteVertex,const int w) {
        EDGE newEdge;
        newEdge.e_state=BASIC;
        newEdge.v_dest=remoteVertex;
        newEdge.e_weight=w;
        newEdge.e_id=m_SE.size();
        m_SE.push_back(newEdge);
    }

    void wakeUp() {
        assert(m_SN==SLEEPING);
        printf("Vertex %d wake up\n",getVertexNumber());

        m_LN=0;
        m_FN=UNKNOWN;
        enterState(FOUND);
        m_findCount=0;
        m_bestEdge=nil;
        m_testEdge=nil;
        m_inBranch=nil;

        size_t m=findMinEdge(ANY);
        assert(m!=nil);

        MESSAGE msg;
        msg.CONNECTMSG.remoteLN=0;
        m_SE[m].e_state=BRANCH;
        sendMessage(CONNECT,this,m_SE[m],msg);
    }

    bool onConnect(EDGE& j,const int L) {
        if (getState()==SLEEPING) wakeUp();
        MESSAGE msg;
        if (L<getLevel()) {
            j.e_state=BRANCH;
            msg.INITIATEMSG.remoteLN=getLevel();
            msg.INITIATEMSG.remoteFN=getID();
            msg.INITIATEMSG.S=getState();
            sendMessage(INITIATE,this,j,msg);
            if (getState()==FIND) m_findCount++;
        }else {
            if (j.e_state==BASIC)
                return false;
            else {
                msg.INITIATEMSG.remoteLN=getLevel()+1;
                msg.INITIATEMSG.remoteFN=j.e_weight;
                msg.INITIATEMSG.S=FIND;
                sendMessage(INITIATE,this,j,msg);
            }
        }
        return true;
    }

    bool OnInitiate(EDGE& j,const int L,const int F,const PROCESSOR_STATE S) {
        setLevel(L);
        setID(F);
        enterState(S);
        m_inBranch=j.e_id;
        m_bestEdge=nil;
        m_bestWT=oo;
        for (size_t i=0;i<m_SE.size();i++) {
            if (i!=j.e_id && m_SE[i].e_state==BRANCH) {
                MESSAGE msg;
                msg.INITIATEMSG.remoteLN=L;
                msg.INITIATEMSG.remoteFN=F;
                msg.INITIATEMSG.S=S;
                sendMessage(INITIATE,this,m_SE[i],msg);
                if (S==FIND) m_findCount++;
            }
        }
        if (S==FIND) doTest();
        return true;
    }

    void doTest() {
        m_testEdge=findMinEdge(BASIC);
        if (m_testEdge!=nil) {
            MESSAGE msg;
            msg.TESTMSG.remoteLN=getLevel();
            msg.TESTMSG.remoteFN=getID();
            sendMessage(TEST,this,m_SE[m_testEdge],msg);
        }else doReport();
    }

    void doReport() {
        if (m_findCount==0 && m_testEdge==nil) {
            enterState(FOUND);
            MESSAGE msg;
            msg.REPORTMSG.candidateW=m_bestWT;
            sendMessage(REPORT,this,m_SE[m_inBranch],msg);
        }
    }

    bool onTest(EDGE& j,const int L,const int F) {
        if (getState()==SLEEPING) wakeUp();
        if (L>getLevel()) return false;
        MESSAGE msg;
        if (F!=getID()) { // not in the same fragment, then accept it
            sendMessage(ACCEPT,this,j,msg);
        }else { // in the same fragment, we should reject
            if (j.e_state==BASIC) j.e_state=REJECTED;
            if (j.e_id!=m_testEdge)
                sendMessage(REJECT,this,j,msg);
            else // the edge is under test
                doTest(); // find another edge
        }
        return true;
    }

    bool onAccept(EDGE& j) {
        assert(m_testEdge!=nil);
        m_testEdge=nil;
        if (j.e_weight<m_bestWT) {
            m_bestWT=j.e_weight;
            m_bestEdge=j.e_id;
        }
        doReport();
        return true;
    }

    bool onReject(EDGE& j) {
        assert(m_testEdge!=nil);
        if (j.e_state==BASIC) j.e_state=REJECTED;
        doTest();
        return true;
    }

    bool onReport(EDGE& j,const int w) {
        assert(m_inBranch!=nil);
        if (j.e_id!=m_inBranch) {
            m_findCount--;
            if (w<m_bestWT) {
                m_bestWT=w;
                m_bestEdge=j.e_id;
            }
            doReport();
        }else if (getState()==FIND) return false;
        else {
                if (w>m_bestWT) doChangeRoot(); // the MWOE is on my side
                else if (w==oo && m_bestWT==oo) doHalt(); // no MWOE
        }
        return true;
    }

    void doHalt() {
        printf("!!! Vertex %d halt\n",getVertexNumber());
        system("PAUSE");
    }

    void doChangeRoot() {
        assert(m_bestEdge!=nil);
        if (m_SE[m_bestEdge].e_state==BRANCH) {
            MESSAGE msg;
            sendMessage(CHANGE_ROOT,this,m_SE[m_bestEdge],msg);
        }else {
            MESSAGE msg;
            msg.CONNECTMSG.remoteLN=getLevel();
            sendMessage(CONNECT,this,m_SE[m_bestEdge],msg);
            m_SE[m_bestEdge].e_state=BRANCH;
        }
    }

    bool onChangeRoot(EDGE& j) {
        doChangeRoot();
        return true;
    }
};

PROCESSOR **v=NULL;
bool newMessage;

bool PROCESSOR::sendMessage(const MESSAGE_TYPE type,const PROCESSOR * const me,const EDGE& edge,MESSAGE& msg) {
    printf("message V%d -> V%d: ",me->getVertexNumber(),edge.v_dest);
    switch (type) {
    case CONNECT:
        printf("Connect(L=%d)\n",msg.CONNECTMSG.remoteLN);
        break;
    case INITIATE:
        printf("Initiate(L=%d,F=%d,S=%s)\n",msg.INITIATEMSG.remoteLN,msg.INITIATEMSG.remoteFN,getStateDesc(msg.INITIATEMSG.S));
        break;
    case TEST:
        printf("Test(L=%d,F=%d)\n",msg.TESTMSG.remoteLN,msg.TESTMSG.remoteFN);
        break;
    case ACCEPT:
        puts("Accept");
        break;
    case REJECT:
        puts("Reject");
        break;
    case REPORT:
        printf("Report(w=%d)\n",msg.REPORTMSG.candidateW);
        break;
    case CHANGE_ROOT:
        puts("ChangeRoot");
        break;
    }
    // prepare
    msg.type=type;
    msg.v_src=me->getVertexNumber();
    // push it to the target queue
    v[edge.v_dest]->pushMessage(msg);
    newMessage=true;
    return true;
}

template <class T>
void SAFE_DELETE(T& x) {
    delete x;
    x=NULL;
}

bool messageloop(const int n) {
    newMessage=false;
    puts("---------------");
    for (int i=1;i<=n;i++)
        if (v[i]->processMessage())
            return true;
    return false;
}

int main() {
    FILE *in;
    in=fopen("input.txt","r");
    assert(in!=NULL);
    //freopen("output.txt","wt",stdout);
    int n,m;
    fscanf(in,"%d%d",&n,&m);
    v=new PROCESSOR*[n+1];
    int i;
    v[0]=NULL;
    for (int i=1;i<=n;i++) v[i]=new PROCESSOR(i);
    for (int i=1;i<=m;i++) {
        int a,b,w;
        fscanf(in,"%d%d%d",&a,&b,&w);
        v[a]->addEdge(b,w);
        v[b]->addEdge(a,w);
    }
    v[1]->wakeUp();

    while (messageloop(n) || newMessage);

    for (i=1;i<=n;i++) SAFE_DELETE(v[i]);
    delete []v;v=NULL;
    return 0;
}
