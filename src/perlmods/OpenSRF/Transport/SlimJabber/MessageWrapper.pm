package OpenSRF::Transport::SlimJabber::MessageWrapper;
use strict; use warnings;
use OpenSRF::Transport::SlimJabber::XMPPMessage;

# ----------------------------------------------------------
# Legacy wrapper for XMPPMessage
# ----------------------------------------------------------

sub new {
	my $class = shift;
    my $msg = shift;
    return bless({msg => $msg}, ref($class) || $class);
}

sub msg {
    my($self, $msg) = @_;
    $self->{msg} = $msg if $msg;
    return $self->{msg};
}

sub toString {
    return $_[0]->msg->to_xml;
}

sub get_body {
    return $_[0]->msg->body;
}

sub get_sess_id {
    return $_[0]->msg->thread;
}

sub get_msg_type {
    return $_[0]->msg->type;
}

sub get_remote_id {
    return $_[0]->msg->from;
}

sub setType {
    $_[0]->msg->type(shift());
}

sub setTo {
    $_[0]->msg->to(shift());
}

sub setThread {
    $_[0]->msg->thread(shift());
}

sub setBody {
    $_[0]->msg->body(shift());
}

sub set_router_command {
    $_[0]->msg->router_command(shift());
}
sub set_router_class {
    $_[0]->msg->router_class(shift());
}

sub set_osrf_xid {
    $_[0]->msg->osrf_xid(shift());
}

sub get_osrf_xid {
   return $_[0]->msg->osrf_xid;
}

1;
